
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9562

int main(void)
{
    int server_fd;
    struct sockaddr_in server_addr;

    // Step 1: Create a TCP socket.
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // Allow the server port to be reused after restarting.
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // Step 2: Configure the server address.
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // Step 3: Bind the socket to the personalised port.
    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // Step 4: Listen for incoming connections.
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("NetMessenger server started.\n");
    printf("Listening on TCP port %d\n", PORT);
    printf("Node ID: NID:7635\n");
    fflush(stdout);

    // Step 5: Accept incoming client connections.
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);

        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        close(client_fd);
        printf("Client connection closed.\n");
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
