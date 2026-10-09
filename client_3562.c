
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9562

int main(void)
{
    int client_fd;
    struct sockaddr_in server_addr;

    // Step 1: Create a TCP socket.
    client_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (client_fd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // Step 2: Configure the server address.
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP,
                  &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP address.\n");
        close(client_fd);
        return EXIT_FAILURE;
    }

    // Step 3: Connect to the server.
    printf("Connecting to NetMessenger server...\n");

    if (connect(client_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_fd);
        return EXIT_FAILURE;
    }

    printf("Connected successfully to %s:%d\n",
           SERVER_IP, SERVER_PORT);

    // Registration and messaging will be implemented later.
    close(client_fd);
    printf("Client disconnected.\n");

    return EXIT_SUCCESS;
}
