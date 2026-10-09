#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define PORT 9562
#define MAX_LINE 4096
#define MAX_FILE_SIZE (10ULL * 1024ULL * 1024ULL)

static int sockfd = -1;

static int send_all(const void *data, size_t length) {
    const unsigned char *p = data;
    while (length) {
        ssize_t n = send(sockfd, p, length, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        length -= (size_t)n;
    }
    return 0;
}

/* A separate thread displays responses and unsolicited messages. */
static void *receive_messages(void *unused) {
    (void)unused;
    char buffer[4096];
    for (;;) {
        ssize_t n = recv(sockfd, buffer, sizeof buffer, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n) break;
        fflush(stdout);
    }
    fprintf(stderr, "\n[Disconnected from server]\n");
    return NULL;
}

static int good_filename(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n > 200 || !strcmp(name, ".") || !strcmp(name, "..")) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_'))
            return 0;
    }
    return 1;
}

/* Usage at the client prompt: SENDFILE <username-or-room> <local-file-path> */
static int upload_file(const char *args) {
    char target[64], path[2048], extra;
    if (sscanf(args, " %63s %2047s %c", target, path, &extra) != 2) {
        fprintf(stderr, "Usage: SENDFILE <target> <local_file_path>\n");
        return 0;
    }
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    if (!good_filename(filename)) {
        fprintf(stderr, "Invalid filename (use letters, digits, dots, hyphens, underscores).\n");
        return 0;
    }
    FILE *file = fopen(path, "rb");
    if (!file) { perror("fopen"); return 0; }
    struct stat st;
    if (fstat(fileno(file), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        fprintf(stderr, "Not a readable regular file.\n");
        fclose(file); return 0;
    }
    uint64_t size = (uint64_t)st.st_size;
    if (size > MAX_FILE_SIZE) {
        fprintf(stderr, "File exceeds the 10 MiB limit.\n");
        fclose(file); return 0;
    }
    char header[2400];
    int len = snprintf(header, sizeof header, "SENDFILE %s %s %" PRIu64 "\n",
                       target, filename, size);
    if (len < 0 || (size_t)len >= sizeof header) {
        fclose(file); return 0;
    }
    if (send_all(header, (size_t)len) != 0) {
        perror("send header"); fclose(file); return -1;
    }
    unsigned char buffer[8192];
    uint64_t remaining = size;
    while (remaining) {
        size_t amount = remaining < sizeof buffer ? (size_t)remaining : sizeof buffer;
        size_t n = fread(buffer, 1, amount, file);
        if (n == 0 || send_all(buffer, n) != 0) {
            fprintf(stderr, "Upload interrupted; reconnect before sending more commands.\n");
            fclose(file); return -1;
        }
        remaining -= n;
    }
    fclose(file);
    printf("[Uploaded %s: %" PRIu64 " bytes to target %s; awaiting server confirmation]\n",
           filename, size, target);
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : DEFAULT_HOST;
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [IPv4_server_address]\n", argv[0]);
        return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return EXIT_FAILURE; }
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", host);
        close(sockfd); return EXIT_FAILURE;
    }
    if (connect(sockfd, (struct sockaddr *)&address, sizeof address) != 0) {
        perror("connect"); close(sockfd); return EXIT_FAILURE;
    }
    printf("NetMessenger Client | %s:%d | NID:7635\n", host, PORT);
    puts("First enter: REGISTER <username>");
    puts("Commands: LIST, BCAST, PMSG, JOIN, LEAVE, ROOMS, RMSG, QUIT");
    puts("File upload: SENDFILE <target> <local_file_path>");
    fflush(stdout);
    pthread_t receiver;
    int rc = pthread_create(&receiver, NULL, receive_messages, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        close(sockfd); return EXIT_FAILURE;
    }
    char *line = NULL;
    size_t capacity = 0;
    while (getline(&line, &capacity, stdin) != -1) {
        size_t length = strlen(line);
        if (length && line[length-1] == '\n') line[--length] = '\0';
        if (length && line[length-1] == '\r') line[--length] = '\0';
        if (!length) continue;
        if (!strncmp(line, "SENDFILE ", 9)) {
            if (upload_file(line + 9) < 0) break;
            continue;
        }
        if (length + 1 >= MAX_LINE) {
            fprintf(stderr, "Command is too long.\n");
            continue;
        }
        if (send_all(line, length) != 0 || send_all("\n", 1) != 0) {
            perror("send"); break;
        }
        if (!strcmp(line, "QUIT")) break;
    }
    free(line);
    shutdown(sockfd, SHUT_WR);
    pthread_join(receiver, NULL);
    close(sockfd);
    return EXIT_SUCCESS;
}
