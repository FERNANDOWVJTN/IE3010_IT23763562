
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9562
#define BACKLOG 10
#define BUFFER_SIZE 1024

static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
static int active_clients = 0;

typedef struct {
    int socket_fd;
    struct sockaddr_in address;
} ClientInfo;

/* Handle each connected client in a separate thread. */
static void *handle_client(void *arg)
{
    ClientInfo *client = (ClientInfo *)arg;
    int client_fd = client->socket_fd;
    char ip_address[INET_ADDRSTRLEN];
    char buffer[BUFFER_SIZE];

    inet_ntop(AF_INET, &client->address.sin_addr,
              ip_address, sizeof(ip_address));

    int client_port = ntohs(client->address.sin_port);

    pthread_mutex_lock(&client_mutex);
    active_clients++;
    printf("[CONNECTED] %s:%d | Active clients: %d\n",
           ip_address, client_port, active_clients);
    fflush(stdout);
    pthread_mutex_unlock(&client_mutex);

    /*
     * Keep the connection open until the client
     * disconnects or a socket error occurs.
     *
     * Protocol command processing will be added later.
     */
    while (1) {
        ssize_t bytes_received =
            recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_received == 0) {
            break;
        }

        if (bytes_received < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("Client receive error");
            break;
        }

        /* Incoming data is currently discarded.
           The command parser will be implemented later. */
    }

    close(client_fd);

    pthread_mutex_lock(&client_mutex);
    active_clients--;
    printf("[DISCONNECTED] %s:%d | Active clients: %d\n",
           ip_address, client_port, active_clients);
    fflush(stdout);
    pthread_mutex_unlock(&client_mutex);

    free(client);
    return NULL;
}

int main(void)
{
    int server_fd;
    int opt = 1;
    struct sockaddr_in server_addr;

    /* Prevent socket writes from terminating the process. */
    signal(SIGPIPE, SIG_IGN);

    /* Step 1: Create the TCP listening socket. */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    /* Step 2: Allow reuse of the server address. */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    /* Step 3: Configure server address and port. */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    /* Step 4: Bind the socket. */
    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    /* Step 5: Listen for incoming connections. */
    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("====================================\n");
    printf("     NetMessenger TCP Server\n");
    printf("====================================\n");
    printf("Listening on TCP port: %d\n", PORT);
    printf("Node ID: NID:7635\n");
    printf("Concurrency Model: POSIX Threads\n");
    printf("Waiting for client connections...\n");
    printf("====================================\n");
    fflush(stdout);

    /* Step 6: Accept multiple clients. */
    while (1) {
        ClientInfo *client = malloc(sizeof(ClientInfo));

        if (client == NULL) {
            perror("Memory allocation failed");
            continue;
        }

        socklen_t client_len = sizeof(client->address);

        client->socket_fd =
            accept(server_fd,
                   (struct sockaddr *)&client->address,
                   &client_len);

        if (client->socket_fd < 0) {
            if (errno != EINTR) {
                perror("Accept failed");
            }

            free(client);
            continue;
        }

        /* Step 7: Create a thread for this client. */
        pthread_t thread_id;

        int result = pthread_create(&thread_id, NULL,
                                    handle_client, client);

        if (result != 0) {
            fprintf(stderr, "Thread creation failed: %s\n",
                    strerror(result));
            close(client->socket_fd);
            free(client);
            continue;
        }

        /* Thread resources are released when it finishes. */
        result = pthread_detach(thread_id);

        if (result != 0) {
            fprintf(stderr, "Thread detach failed: %s\n",
                    strerror(result));
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
