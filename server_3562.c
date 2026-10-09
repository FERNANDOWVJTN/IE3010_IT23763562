
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9562
#define NODE_ID "NID:7635"
#define BACKLOG 10
#define MAX_CLIENTS 64
#define MAX_USERNAME 31
#define MAX_LINE 4096

typedef struct Client {
    int fd;
    char username[MAX_USERNAME + 1];
    int registered;
    struct Client *next;
} Client;

static Client *clients = NULL;
static pthread_mutex_t clients_lock =
    PTHREAD_MUTEX_INITIALIZER;
static int connected_count = 0;

/* Send all bytes even when send() is partial. */
static int send_all(int fd, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent,
                         MSG_NOSIGNAL);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

/* Caller must hold the clients mutex. */
static void send_locked(Client *client, const char *message)
{
    if (send_all(client->fd, message, strlen(message)) < 0) {
        /* The client thread handles cleanup later. */
    }
}

static void reply(Client *client, const char *message)
{
    pthread_mutex_lock(&clients_lock);
    send_locked(client, message);
    pthread_mutex_unlock(&clients_lock);
}

/* Read one complete newline-terminated command. */
static int read_line(int fd, char *buffer, size_t capacity)
{
    size_t length = 0;
    int too_long = 0;

    while (1) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);

        if (n == 0)
            return 0;

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (ch == '\n') {
            if (too_long)
                return -2;

            if (length > 0 && buffer[length - 1] == '\r')
                length--;

            buffer[length] = '\0';
            return 1;
        }

        if (!too_long && length + 1 < capacity) {
            buffer[length++] = ch;
        } else {
            too_long = 1;
        }
    }
}

static int valid_username(const char *name)
{
    size_t length = strlen(name);

    if (length == 0 || length > MAX_USERNAME)
        return 0;

    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)name[i];

        if (!isalnum(ch) && ch != '_')
            return 0;
    }

    return 1;
}

/* Must be called while holding clients_lock. */
static int username_taken(const char *name)
{
    for (Client *p = clients; p != NULL; p = p->next) {
        if (p->registered &&
            strcmp(p->username, name) == 0)
            return 1;
    }

    return 0;
}

/* Find a registered user. Mutex must be held. */
static Client *find_user(const char *username)
{
    for (Client *p = clients; p != NULL; p = p->next) {
        if (p->registered &&
            strcmp(p->username, username) == 0)
            return p;
    }

    return NULL;
}

/* Send presence events to other registered clients. */
static void notify_others(Client *sender, const char *event)
{
    char message[128];

    snprintf(message, sizeof(message),
             "MSG %s %s\n", event, sender->username);

    for (Client *p = clients; p != NULL; p = p->next) {
        if (p != sender && p->registered)
            send_locked(p, message);
    }
}

/* Register a unique username. */
static void register_user(Client *client, const char *name)
{
    if (!valid_username(name)) {
        reply(client,
              "ERR 005 INVALID_USERNAME " NODE_ID "\n");
        return;
    }

    pthread_mutex_lock(&clients_lock);

    if (username_taken(name)) {
        send_locked(client,
                    "ERR 001 USERNAME_TAKEN " NODE_ID "\n");
    } else {
        strcpy(client->username, name);
        client->registered = 1;

        char response[128];

        snprintf(response, sizeof(response),
                 "OK REGISTERED %s " NODE_ID "\n", name);

        send_locked(client, response);
        notify_others(client, "JOIN");

        printf("[REGISTERED] %s\n", name);
        fflush(stdout);
    }

    pthread_mutex_unlock(&clients_lock);
}

/* List all registered users. */
static void list_users(Client *client)
{
    char response[MAX_LINE];
    size_t used;
    int first = 1;

    pthread_mutex_lock(&clients_lock);

    used = (size_t)snprintf(response, sizeof(response),
                            "OK USERS ");

    for (Client *p = clients; p != NULL; p = p->next) {
        if (!p->registered)
            continue;

        size_t length = strlen(p->username);

        if (used + length + 32 >= sizeof(response))
            break;

        if (!first)
            response[used++] = ',';

        memcpy(response + used, p->username, length);
        used += length;
        first = 0;
    }

    snprintf(response + used, sizeof(response) - used,
             " " NODE_ID "\n");

    send_locked(client, response);
    pthread_mutex_unlock(&clients_lock);
}

/* Send a message to all other registered users. */
static void broadcast_message(Client *sender, const char *text)
{
    if (text[0] == '\0') {
        reply(sender,
              "ERR 005 EMPTY_MESSAGE " NODE_ID "\n");
        return;
    }

    char forwarded[MAX_LINE + MAX_USERNAME + 32];

    snprintf(forwarded, sizeof(forwarded),
             "MSG BCAST %s %s\n",
             sender->username, text);

    pthread_mutex_lock(&clients_lock);

    for (Client *p = clients; p != NULL; p = p->next) {
        if (p->registered && p != sender)
            send_locked(p, forwarded);
    }

    send_locked(sender, "OK SENT " NODE_ID "\n");

    printf("[BCAST] %s: %s\n",
           sender->username, text);
    fflush(stdout);

    pthread_mutex_unlock(&clients_lock);
}

/*
 * Send a private message to exactly one registered user.
 *
 * Sender response: OK SENT NID:7635
 * Target receives: MSG PRIV <sender> <message>
 */
static void private_message(Client *sender, const char *args)
{
    const char *space = strchr(args, ' ');

    if (space == NULL || space == args ||
        space[1] == '\0') {
        reply(sender,
              "ERR 005 INVALID_PMSG " NODE_ID "\n");
        return;
    }

    size_t username_length = (size_t)(space - args);

    if (username_length > MAX_USERNAME) {
        reply(sender,
              "ERR 002 USER_NOT_FOUND " NODE_ID "\n");
        return;
    }

    char target_name[MAX_USERNAME + 1];
    memcpy(target_name, args, username_length);
    target_name[username_length] = '\0';

    const char *message_text = space + 1;

    pthread_mutex_lock(&clients_lock);

    Client *target = find_user(target_name);

    if (target == NULL) {
        send_locked(sender,
                    "ERR 002 USER_NOT_FOUND " NODE_ID "\n");
    } else {
        char forwarded[MAX_LINE + MAX_USERNAME + 32];

        snprintf(forwarded, sizeof(forwarded),
                 "MSG PRIV %s %s\n",
                 sender->username, message_text);

        send_locked(target, forwarded);
        send_locked(sender, "OK SENT " NODE_ID "\n");

        printf("[PMSG] %s -> %s\n",
               sender->username, target_name);
        fflush(stdout);
    }

    pthread_mutex_unlock(&clients_lock);
}

/* Process text commands. Return 0 to disconnect. */
static int process_command(Client *client, const char *line)
{
    if (!client->registered) {
        if (strncmp(line, "REGISTER ", 9) == 0) {
            register_user(client, line + 9);
        } else {
            reply(client,
                  "ERR 005 REGISTER_REQUIRED " NODE_ID "\n");
        }

        return 1;
    }

    if (strcmp(line, "LIST") == 0) {
        list_users(client);
    }
    else if (strcmp(line, "QUIT") == 0) {
        reply(client, "OK BYE " NODE_ID "\n");
        return 0;
    }
    else if (strncmp(line, "BCAST ", 6) == 0) {
        broadcast_message(client, line + 6);
    }
    else if (strcmp(line, "BCAST") == 0) {
        reply(client,
              "ERR 005 EMPTY_MESSAGE " NODE_ID "\n");
    }
    else if (strncmp(line, "PMSG ", 5) == 0) {
        private_message(client, line + 5);
    }
    else if (strcmp(line, "PMSG") == 0) {
        reply(client,
              "ERR 005 INVALID_PMSG " NODE_ID "\n");
    }
    else if (strncmp(line, "REGISTER", 8) == 0) {
        reply(client,
              "ERR 005 ALREADY_REGISTERED " NODE_ID "\n");
    }
    else {
        reply(client,
              "ERR 005 UNKNOWN_COMMAND " NODE_ID "\n");
    }

    return 1;
}

/* Separate execution thread for each client. */
static void *handle_client(void *arg)
{
    Client *client = (Client *)arg;
    char line[MAX_LINE];

    while (1) {
        int status = read_line(client->fd,
                               line, sizeof(line));

        if (status == 0)
            break;

        if (status == -1) {
            perror("Client receive error");
            break;
        }

        if (status == -2) {
            reply(client,
                  "ERR 005 LINE_TOO_LONG " NODE_ID "\n");
            continue;
        }

        if (!process_command(client, line))
            break;
    }

    pthread_mutex_lock(&clients_lock);

    if (client->registered) {
        notify_others(client, "LEAVE");

        printf("[LEFT] %s\n", client->username);
    }

    Client **current = &clients;

    while (*current != NULL) {
        if (*current == client) {
            *current = client->next;
            break;
        }

        current = &(*current)->next;
    }

    connected_count--;

    printf("[DISCONNECTED] Active clients: %d\n",
           connected_count);
    fflush(stdout);

    pthread_mutex_unlock(&clients_lock);

    close(client->fd);
    free(client);
    return NULL;
}

int main(void)
{
    int server_fd;
    int opt = 1;
    struct sockaddr_in server_addr;

    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("====================================\n");
    printf("        NetMessenger Server\n");
    printf("====================================\n");
    printf("Listening on TCP port: %d\n", PORT);
    printf("Node ID: %s\n", NODE_ID);
    printf("Concurrency Model: POSIX Threads\n");
    printf("Commands: REGISTER, LIST, BCAST, PMSG, QUIT\n");
    printf("Waiting for connections...\n");
    printf("====================================\n");
    fflush(stdout);

    while (1) {
        struct sockaddr_in address;
        socklen_t address_len = sizeof(address);

        int fd = accept(server_fd,
                        (struct sockaddr *)&address,
                        &address_len);

        if (fd < 0) {
            if (errno != EINTR)
                perror("Accept failed");
            continue;
        }

        Client *client = calloc(1, sizeof(Client));

        if (client == NULL) {
            perror("Memory allocation failed");
            close(fd);
            continue;
        }

        client->fd = fd;

        pthread_mutex_lock(&clients_lock);

        if (connected_count >= MAX_CLIENTS) {
            const char *error =
                "ERR 005 SERVER_FULL " NODE_ID "\n";

            send_all(fd, error, strlen(error));

            pthread_mutex_unlock(&clients_lock);
            close(fd);
            free(client);
            continue;
        }

        client->next = clients;
        clients = client;
        connected_count++;

        printf("[CONNECTED] Active clients: %d\n",
               connected_count);
        fflush(stdout);

        pthread_mutex_unlock(&clients_lock);

        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL,
                                    handle_client, client);

        if (result != 0) {
            fprintf(stderr,
                    "Thread creation failed: %s\n",
                    strerror(result));

            pthread_mutex_lock(&clients_lock);

            Client **p = &clients;

            while (*p != NULL) {
                if (*p == client) {
                    *p = client->next;
                    break;
                }

                p = &(*p)->next;
            }

            connected_count--;
            pthread_mutex_unlock(&clients_lock);

            close(fd);
            free(client);
            continue;
        }

        result = pthread_detach(thread_id);

        if (result != 0) {
            fprintf(stderr,
                    "Thread detach failed: %s\n",
                    strerror(result));
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
