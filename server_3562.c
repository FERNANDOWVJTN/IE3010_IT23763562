
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
#define MAX_LINE 2048

typedef struct Client {
    int fd;
    char username[MAX_USERNAME + 1];
    int registered;
    struct Client *next;
} Client;

static Client *clients = NULL;
static pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
static int connected_count = 0;

/* Send all bytes, including when send() is partial. */
static int send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent,
                         MSG_NOSIGNAL);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

/* Must be called while clients_lock is held. */
static void send_locked(Client *client, const char *text)
{
    if (send_all(client->fd, text, strlen(text)) < 0) {
        /* The client thread will detect disconnection. */
    }
}

static void reply(Client *client, const char *text)
{
    pthread_mutex_lock(&clients_lock);
    send_locked(client, text);
    pthread_mutex_unlock(&clients_lock);
}

/*
 * Read exactly one newline-terminated text command.
 * Partial TCP receives and combined commands are handled.
 * Returns 1 for a line, 0 for disconnect, -1 for error,
 * and -2 for an oversized line.
 */
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

        if (length + 1 < capacity && !too_long) {
            buffer[length++] = ch;
        } else {
            too_long = 1;
        }
    }
}

static int valid_username(const char *name)
{
    size_t len = strlen(name);

    if (len == 0 || len > MAX_USERNAME)
        return 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)name[i];

        if (!isalnum(ch) && ch != '_')
            return 0;
    }

    return 1;
}

/* Check for a username while holding the mutex. */
static int username_taken(const char *name)
{
    for (Client *p = clients; p != NULL; p = p->next) {
        if (p->registered &&
            strcmp(p->username, name) == 0)
            return 1;
    }

    return 0;
}

/* Notify other registered users about presence. */
static void notify_others(Client *sender, const char *event)
{
    char message[128];

    snprintf(message, sizeof(message), "MSG %s %s\n",
             event, sender->username);

    for (Client *p = clients; p != NULL; p = p->next) {
        if (p != sender && p->registered)
            send_locked(p, message);
    }
}

/* Handle REGISTER as the first command. */
static void register_user(Client *client, const char *name)
{
    if (!valid_username(name)) {
        reply(client, "ERR 005 INVALID_USERNAME " NODE_ID "\n");
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

/* Return the list of registered users. */
static void list_users(Client *client)
{
    char response[MAX_LINE];
    size_t used = 0;
    int first = 1;

    pthread_mutex_lock(&clients_lock);

    used = (size_t)snprintf(response, sizeof(response),
                            "OK USERS ");

    for (Client *p = clients; p != NULL; p = p->next) {
        if (!p->registered)
            continue;

        size_t len = strlen(p->username);

        /* Reserve space for comma, NID, newline and NUL. */
        if (used + len + 32 >= sizeof(response))
            break;

        if (!first)
            response[used++] = ',';

        memcpy(response + used, p->username, len);
        used += len;
        first = 0;
    }

    snprintf(response + used, sizeof(response) - used,
             " " NODE_ID "\n");

    send_locked(client, response);
    pthread_mutex_unlock(&clients_lock);
}

/* Process a text command. Return 0 when quitting. */
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
    } else if (strcmp(line, "QUIT") == 0) {
        reply(client, "OK BYE " NODE_ID "\n");
        return 0;
    } else if (strncmp(line, "REGISTER", 8) == 0) {
        reply(client,
              "ERR 005 ALREADY_REGISTERED " NODE_ID "\n");
    } else {
        reply(client,
              "ERR 005 UNKNOWN_COMMAND " NODE_ID "\n");
    }

    return 1;
}

static void *handle_client(void *arg)
{
    Client *client = (Client *)arg;
    char line[MAX_LINE];

    while (1) {
        int status = read_line(client->fd, line, sizeof(line));

        if (status == 0)
            break;

        if (status == -1) {
            perror("Client receive error");
            break;
        }

        if (status == -2) {
            reply(client, "ERR 005 LINE_TOO_LONG " NODE_ID "\n");
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

    /* Remove this connection from the shared client list. */
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

    printf("NetMessenger Server\n");
    printf("Listening on port: %d\n", PORT);
    printf("Node ID: %s\n", NODE_ID);
    printf("Concurrency: POSIX Threads\n");
    printf("Waiting for clients...\n");
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
            send_all(fd,
                     "ERR 005 SERVER_FULL " NODE_ID "\n",
                     strlen("ERR 005 SERVER_FULL " NODE_ID "\n"));

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
            fprintf(stderr, "Thread creation failed: %s\n",
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
            fprintf(stderr, "Thread detach failed: %s\n",
                    strerror(result));
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
