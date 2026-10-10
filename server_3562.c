#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 9562
#define NODE_ID "NID:7635"
#define MAX_CLIENTS 64
#define MAX_ROOMS 64
#define MAX_NAME 31
#define MAX_LINE 4096
#define MAX_FILE_SIZE (10ULL * 1024ULL * 1024ULL)
#define STORAGE_ROOT "storage/IT23763562"
#define LOG_FILE "netmsg_IT23763562.log"

typedef struct Client {
    int fd;
    int registered;
    char username[MAX_NAME + 1];
    struct Client *next;
} Client;
typedef struct Member {
    Client *client;
    struct Member *next;
} Member;
typedef struct Room {
    char name[MAX_NAME + 1];
    Member *members;
    struct Room *next;
} Room;

static Client *clients = NULL;
static Room *rooms = NULL;
static int client_count = 0, room_count = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_file = NULL;

/* Independent logging mutex: safe even when the client-state lock is held. */
static void log_event(const char *tag, const char *fmt, ...) {
    char timestamp[32] = "unknown-time";
    time_t now = time(NULL);
    struct tm local_tm;
    if (localtime_r(&now, &local_tm))
        strftime(timestamp, sizeof timestamp, "%Y-%m-%d %H:%M:%S", &local_tm);

    pthread_mutex_lock(&log_lock);
    va_list ap;
    printf("[%s] [%s] ", timestamp, tag);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    fflush(stdout);
    if (log_file) {
        fprintf(log_file, "[%s] [%s] ", timestamp, tag);
        va_start(ap, fmt);
        vfprintf(log_file, fmt, ap);
        va_end(ap);
        fputc('\n', log_file);
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_lock);
}

static int send_all(int fd, const void *data, size_t length) {
    const unsigned char *p = data;
    while (length) {
        ssize_t n = send(fd, p, length, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        length -= (size_t)n;
    }
    return 0;
}
/* All writes to clients take the same lock, preventing interleaved messages. */
static void send_locked(Client *c, const char *s) {
    (void)send_all(c->fd, s, strlen(s));
}
static void reply(Client *c, const char *s) {
    pthread_mutex_lock(&lock);
    send_locked(c, s);
    pthread_mutex_unlock(&lock);
}
/* Reading one byte at a time avoids consuming file payload as command text. */
static int read_line(int fd, char *out, size_t capacity) {
    size_t i = 0;
    int overflow = 0;
    for (;;) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (ch == '\n') {
            if (overflow) return -2;
            if (i && out[i - 1] == '\r') --i;
            out[i] = '\0';
            return 1;
        }
        if (!overflow && i + 1 < capacity) out[i++] = ch;
        else overflow = 1;
    }
}
static int valid_name(const char *s) {
    size_t n = strlen(s);
    if (!n || n > MAX_NAME) return 0;
    for (size_t i = 0; i < n; ++i)
        if (!isalnum((unsigned char)s[i]) && s[i] != '_') return 0;
    return 1;
}
static int valid_filename(const char *s) {
    size_t n = strlen(s);
    if (!n || n > 200 || !strcmp(s, ".") || !strcmp(s, "..")) return 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (!isalnum(ch) && ch != '.' && ch != '_' && ch != '-') return 0;
    }
    return 1;
}
/* Lookup functions below require lock to be held. */
static Client *find_user(const char *name) {
    for (Client *c = clients; c; c = c->next)
        if (c->registered && !strcmp(c->username, name)) return c;
    return NULL;
}
static Room *find_room(const char *name) {
    for (Room *r = rooms; r; r = r->next)
        if (!strcmp(r->name, name)) return r;
    return NULL;
}
static int member_of(Room *room, Client *c) {
    for (Member *m = room->members; m; m = m->next)
        if (m->client == c) return 1;
    return 0;
}
static void delete_if_empty(Room *room) {
    if (room->members) return;
    Room **p = &rooms;
    while (*p && *p != room) p = &(*p)->next;
    if (*p) {
        log_event("ROOM DELETE", "%s", room->name);
        *p = room->next;
        free(room);
        --room_count;
    }
}
static void notify_presence(Client *c, const char *event) {
    char msg[96];
    snprintf(msg, sizeof msg, "MSG %s %s\n", event, c->username);
    for (Client *p = clients; p; p = p->next)
        if (p != c && p->registered) send_locked(p, msg);
}
static void register_user(Client *c, const char *name) {
    if (!valid_name(name)) {
        log_event("ERROR", "Invalid username registration attempt");
        reply(c, "ERR 005 INVALID_USERNAME " NODE_ID "\n"); return;
    }
    pthread_mutex_lock(&lock);
    if (find_user(name)) {
        send_locked(c, "ERR 001 USERNAME_TAKEN " NODE_ID "\n");
        log_event("ERROR", "Duplicate username: %s", name);
    } else {
        char msg[96];
        strcpy(c->username, name);
        c->registered = 1;
        snprintf(msg, sizeof msg, "OK REGISTERED %s " NODE_ID "\n", name);
        send_locked(c, msg);
        notify_presence(c, "JOIN");
        log_event("REGISTERED", "%s", name);
    }
    pthread_mutex_unlock(&lock);
}
static void list_users(Client *c) {
    char msg[MAX_LINE];
    size_t n = (size_t)snprintf(msg, sizeof msg, "OK USERS ");
    int first = 1;
    pthread_mutex_lock(&lock);
    for (Client *p = clients; p; p = p->next) {
        if (!p->registered) continue;
        size_t z = strlen(p->username);
        if (n + z + 32 >= sizeof msg) break;
        if (!first) msg[n++] = ',';
        memcpy(msg + n, p->username, z);
        n += z; first = 0;
    }
    snprintf(msg + n, sizeof msg - n, " " NODE_ID "\n");
    send_locked(c, msg);
    log_event("LIST", "Requested by %s", c->username);
    pthread_mutex_unlock(&lock);
}
static void broadcast(Client *c, const char *body) {
    if (!*body) { reply(c, "ERR 005 EMPTY_MESSAGE " NODE_ID "\n"); return; }
    char msg[MAX_LINE + 64];
    snprintf(msg, sizeof msg, "MSG BCAST %s %s\n", c->username, body);
    pthread_mutex_lock(&lock);
    for (Client *p = clients; p; p = p->next)
        if (p != c && p->registered) send_locked(p, msg);
    send_locked(c, "OK SENT " NODE_ID "\n");
    log_event("BCAST", "%s: %s", c->username, body);
    pthread_mutex_unlock(&lock);
}
static void private_message(Client *c, const char *args) {
    const char *sp = strchr(args, ' ');
    if (!sp || sp == args || !sp[1]) {
        reply(c, "ERR 005 INVALID_PMSG " NODE_ID "\n"); return;
    }
    size_t n = (size_t)(sp - args);
    if (n > MAX_NAME) {
        reply(c, "ERR 002 USER_NOT_FOUND " NODE_ID "\n"); return;
    }
    char who[MAX_NAME + 1], msg[MAX_LINE + 64];
    memcpy(who, args, n); who[n] = '\0';
    pthread_mutex_lock(&lock);
    Client *target = find_user(who);
    if (!target) {
        send_locked(c, "ERR 002 USER_NOT_FOUND " NODE_ID "\n");
        log_event("ERROR", "PMSG target not found: %s -> %s", c->username, who);
    } else {
        snprintf(msg, sizeof msg, "MSG PRIV %s %s\n", c->username, sp + 1);
        send_locked(target, msg);
        send_locked(c, "OK SENT " NODE_ID "\n");
        log_event("PMSG", "%s -> %s", c->username, who);
    }
    pthread_mutex_unlock(&lock);
}
static void join_room(Client *c, const char *name) {
    if (!valid_name(name)) { reply(c, "ERR 005 INVALID_ROOM " NODE_ID "\n"); return; }
    pthread_mutex_lock(&lock);
    Room *r = find_room(name);
    if (!r) {
        if (room_count >= MAX_ROOMS) {
            send_locked(c, "ERR 005 ROOM_LIMIT " NODE_ID "\n");
            log_event("ERROR", "Room limit reached");
            pthread_mutex_unlock(&lock); return;
        }
        r = calloc(1, sizeof *r);
        if (!r) {
            send_locked(c, "ERR 005 SERVER_ERROR " NODE_ID "\n");
            log_event("ERROR", "Cannot allocate room");
            pthread_mutex_unlock(&lock); return;
        }
        strcpy(r->name, name); r->next = rooms; rooms = r; ++room_count;
    }
    if (!member_of(r, c)) {
        Member *m = calloc(1, sizeof *m);
        if (!m) {
            delete_if_empty(r);
            send_locked(c, "ERR 005 SERVER_ERROR " NODE_ID "\n");
            log_event("ERROR", "Cannot allocate room membership");
            pthread_mutex_unlock(&lock); return;
        }
        m->client = c; m->next = r->members; r->members = m;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "OK JOINED %s " NODE_ID "\n", name);
    send_locked(c, msg);
    log_event("ROOM JOIN", "%s -> %s", c->username, name);
    pthread_mutex_unlock(&lock);
}
static void leave_room(Client *c, const char *name) {
    pthread_mutex_lock(&lock);
    Room *r = find_room(name);
    if (!r || !member_of(r, c)) {
        send_locked(c, "ERR 003 ROOM_NOT_FOUND " NODE_ID "\n");
        log_event("ERROR", "LEAVE room not found: %s -> %s", c->username, name);
        pthread_mutex_unlock(&lock); return;
    }
    Member **p = &r->members;
    while (*p && (*p)->client != c) p = &(*p)->next;
    if (*p) { Member *old = *p; *p = old->next; free(old); }
    char msg[96];
    snprintf(msg, sizeof msg, "OK LEFT %s " NODE_ID "\n", name);
    send_locked(c, msg);
    log_event("ROOM LEAVE", "%s -> %s", c->username, name);
    delete_if_empty(r);
    pthread_mutex_unlock(&lock);
}
static void list_rooms(Client *c) {
    char msg[MAX_LINE];
    size_t n = (size_t)snprintf(msg, sizeof msg, "OK ROOMS ");
    int first = 1;
    pthread_mutex_lock(&lock);
    for (Room *r = rooms; r; r = r->next) {
        size_t z = strlen(r->name);
        if (n + z + 32 >= sizeof msg) break;
        if (!first) msg[n++] = ',';
        memcpy(msg + n, r->name, z); n += z; first = 0;
    }
    snprintf(msg + n, sizeof msg - n, " " NODE_ID "\n");
    send_locked(c, msg);
    log_event("ROOMS", "Requested by %s", c->username);
    pthread_mutex_unlock(&lock);
}
static void room_message(Client *c, const char *args) {
    const char *sp = strchr(args, ' ');
    if (!sp || sp == args || !sp[1]) {
        reply(c, "ERR 005 INVALID_RMSG " NODE_ID "\n"); return;
    }
    size_t n = (size_t)(sp - args);
    if (n > MAX_NAME) { reply(c, "ERR 003 ROOM_NOT_FOUND " NODE_ID "\n"); return; }
    char name[MAX_NAME + 1], msg[MAX_LINE + 100];
    memcpy(name, args, n); name[n] = '\0';
    pthread_mutex_lock(&lock);
    Room *r = find_room(name);
    if (!r || !member_of(r, c)) {
        send_locked(c, "ERR 003 ROOM_NOT_FOUND " NODE_ID "\n");
        log_event("ERROR", "RMSG room not found or not joined: %s -> %s", c->username, name);
    } else {
        snprintf(msg, sizeof msg, "MSG ROOM %s %s %s\n", name, c->username, sp + 1);
        for (Member *m = r->members; m; m = m->next)
            if (m->client != c && m->client->registered) send_locked(m->client, msg);
        send_locked(c, "OK SENT " NODE_ID "\n");
        log_event("RMSG", "%s -> room %s", c->username, name);
    }
    pthread_mutex_unlock(&lock);
}
static void cleanup_rooms(Client *c) {
    for (Room *r = rooms; r;) {
        Room *next = r->next;
        Member **p = &r->members;
        while (*p) {
            if ((*p)->client == c) {
                Member *old = *p; *p = old->next; free(old);
            } else p = &(*p)->next;
        }
        delete_if_empty(r);
        r = next;
    }
}
static int make_directory(const char *path) {
    if (mkdir(path, 0700) == 0 || errno == EEXIST) {
        struct stat st;
        return stat(path, &st) == 0 && S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return -1;
}
/* Forward a saved file as a header followed by exactly size raw bytes.
 * The global client lock is held over each complete header+payload so that
 * other threads cannot mix chat messages into a binary transfer. */
static int send_stored_file_locked(Client *recipient, const char *sender_name,
                                   const char *filename, FILE *input,
                                   uint64_t size) {
    char header[512];
    int hlen = snprintf(header, sizeof header, "FILE %s %s %" PRIu64 "\n",
                        sender_name, filename, size);
    if (hlen < 0 || (size_t)hlen >= sizeof header) return -1;
    if (send_all(recipient->fd, header, (size_t)hlen) != 0) {
        shutdown(recipient->fd, SHUT_RDWR);
        return -1;
    }
    unsigned char data[8192];
    uint64_t remaining = size;
    while (remaining) {
        size_t need = remaining < sizeof data ? (size_t)remaining : sizeof data;
        size_t n = fread(data, 1, need, input);
        if (n != need || send_all(recipient->fd, data, n) != 0) {
            /* Never continue text messages on a partly transmitted FILE. */
            shutdown(recipient->fd, SHUT_RDWR);
            return -1;
        }
        remaining -= n;
    }
    return 0;
}

static int forward_stored_file(Client *sender, const char *target,
                               const char *filename, const char *path,
                               uint64_t size) {
    FILE *input = fopen(path, "rb");
    if (!input) {
        log_event("ERROR", "Cannot reopen file for delivery: %s", path);
        return 0;
    }
    int delivered = 0;
    pthread_mutex_lock(&lock);
    Client *user = find_user(target);
    if (user && user != sender) {
        if (send_stored_file_locked(user, sender->username, filename,
                                    input, size) == 0) delivered++;
    } else if (!user) {
        Room *room = find_room(target);
        if (room && member_of(room, sender)) {
            for (Member *m = room->members; m; m = m->next) {
                Client *to = m->client;
                if (to == sender || !to->registered) continue;
                if (fseek(input, 0, SEEK_SET) != 0) break;
                if (send_stored_file_locked(to, sender->username, filename,
                                            input, size) == 0) delivered++;
            }
        }
    }
    pthread_mutex_unlock(&lock);
    fclose(input);
    return delivered;
}

/* Return 0 for file command handled; -1 to disconnect after failure. */
static int receive_file(Client *sender, const char *args) {
    char target[MAX_NAME + 1], filename[201], size_text[32], extra;
    uint64_t size;
    if (sscanf(args, "%31s %200s %31s %c", target, filename, size_text, &extra) != 3 ||
        !valid_name(target) || !valid_filename(filename)) {
        log_event("ERROR", "Invalid file header from %s", sender->username);
        reply(sender, "ERR 005 INVALID_FILE_HEADER " NODE_ID "\n");
        return -1; /* Unknown payload length: cannot resume safely. */
    }
    if (size_text[0] == '-' || size_text[0] == '+' || !isdigit((unsigned char)size_text[0])) {
        log_event("ERROR", "Invalid file size from %s", sender->username);
        reply(sender, "ERR 005 INVALID_FILE_SIZE " NODE_ID "\n"); return -1;
    }
    for (size_t i = 0; size_text[i]; ++i)
        if (!isdigit((unsigned char)size_text[i])) {
            log_event("ERROR", "Invalid file size from %s", sender->username);
            reply(sender, "ERR 005 INVALID_FILE_SIZE " NODE_ID "\n"); return -1;
        }
    errno = 0;
    unsigned long long parsed = strtoull(size_text, NULL, 10);
    if (errno == ERANGE) {
        log_event("ERROR", "File too large from %s", sender->username);
        reply(sender, "ERR 004 FILE_TOO_LARGE " NODE_ID "\n"); return -1;
    }
    size = (uint64_t)parsed;
    if (size > MAX_FILE_SIZE) {
        log_event("ERROR", "File too large from %s: %" PRIu64 " bytes", sender->username, size);
        reply(sender, "ERR 004 FILE_TOO_LARGE " NODE_ID "\n"); return -1;
    }
    /* Target can be a registered username OR an existing room. */
    pthread_mutex_lock(&lock);
    Client *user = find_user(target);
    Room *room = find_room(target);
    int allowed = user != NULL || (room != NULL && member_of(room, sender));
    if (!allowed) {
        send_locked(sender, room ? "ERR 003 ROOM_NOT_FOUND " NODE_ID "\n"
                                 : "ERR 002 USER_NOT_FOUND " NODE_ID "\n");
        log_event("ERROR", "File target unavailable: %s -> %s", sender->username, target);
        pthread_mutex_unlock(&lock);
        return -1; /* Do not interpret forthcoming bytes as text. */
    }
    pthread_mutex_unlock(&lock);

    char userdir[256], path[512], temporary[550];
    snprintf(userdir, sizeof userdir, "%s/%s", STORAGE_ROOT, sender->username);
    if (make_directory("storage") != 0 ||
        make_directory(STORAGE_ROOT) != 0 ||
        make_directory(userdir) != 0) {
        log_event("ERROR", "Cannot create storage directory for %s", sender->username);
        reply(sender, "ERR 005 STORAGE_ERROR " NODE_ID "\n"); return -1;
    }
    snprintf(path, sizeof path, "%s/%s", userdir, filename);
    snprintf(temporary, sizeof temporary, "%s/.upload_XXXXXX", userdir);
    int out_fd = mkstemp(temporary);
    if (out_fd < 0) {
        log_event("ERROR", "Cannot create temporary file for %s", sender->username);
        reply(sender, "ERR 005 STORAGE_ERROR " NODE_ID "\n"); return -1;
    }
    unsigned char buffer[8192];
    uint64_t remaining = size;
    int failed = 0;
    while (remaining) {
        size_t need = remaining < sizeof buffer ? (size_t)remaining : sizeof buffer;
        ssize_t n = recv(sender->fd, buffer, need, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { failed = 1; break; }
        size_t written = 0;
        while (written < (size_t)n) {
            ssize_t w = write(out_fd, buffer + written, (size_t)n - written);
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) { failed = 1; break; }
            written += (size_t)w;
        }
        if (failed) break;
        remaining -= (uint64_t)n;
    }
    if (close(out_fd) != 0) failed = 1;
    if (!failed && rename(temporary, path) != 0) failed = 1;
    if (failed) {
        unlink(temporary);
        log_event("ERROR", "File transfer failed: %s -> %s, %s", sender->username, target, filename);
        reply(sender, "ERR 005 FILE_TRANSFER_FAILED " NODE_ID "\n");
        return -1;
    }
    /* Deliver to the named user or all other members of the target room. */
    int delivered = forward_stored_file(sender, target, filename, path, size);
    char response[256];
    if (delivered > 0) {
        snprintf(response, sizeof response, "OK FILE_RECEIVED %s " NODE_ID "\n", filename);
        reply(sender, response);
        log_event("FILE", "%s -> %s: %s (%" PRIu64 " bytes, %d delivery/ies)",
                  sender->username, target, path, size, delivered);
    } else {
        reply(sender, "ERR 005 FILE_DELIVERY_FAILED " NODE_ID "\n");
        log_event("ERROR", "File stored but delivery failed: %s -> %s: %s",
                  sender->username, target, path);
    }
    return 0;
}
/* 1 = continue receiving commands; 0 = close connection. */
static int process_command(Client *c, const char *line) {
    if (!c->registered) {
        if (!strncmp(line, "REGISTER ", 9)) register_user(c, line + 9);
        else {
            log_event("ERROR", "Unregistered client attempted command");
            reply(c, "ERR 005 REGISTER_REQUIRED " NODE_ID "\n");
        }
        return 1;
    }
    if (!strcmp(line, "LIST")) list_users(c);
    else if (!strcmp(line, "QUIT")) { log_event("QUIT", "%s", c->username); reply(c, "OK BYE " NODE_ID "\n"); return 0; }
    else if (!strncmp(line, "BCAST ", 6)) broadcast(c, line + 6);
    else if (!strcmp(line, "BCAST")) reply(c, "ERR 005 EMPTY_MESSAGE " NODE_ID "\n");
    else if (!strncmp(line, "PMSG ", 5)) private_message(c, line + 5);
    else if (!strcmp(line, "PMSG")) reply(c, "ERR 005 INVALID_PMSG " NODE_ID "\n");
    else if (!strncmp(line, "JOIN ", 5)) join_room(c, line + 5);
    else if (!strcmp(line, "JOIN")) reply(c, "ERR 005 INVALID_ROOM " NODE_ID "\n");
    else if (!strncmp(line, "LEAVE ", 6)) leave_room(c, line + 6);
    else if (!strcmp(line, "LEAVE")) reply(c, "ERR 003 ROOM_NOT_FOUND " NODE_ID "\n");
    else if (!strcmp(line, "ROOMS")) list_rooms(c);
    else if (!strncmp(line, "RMSG ", 5)) room_message(c, line + 5);
    else if (!strcmp(line, "RMSG")) reply(c, "ERR 005 INVALID_RMSG " NODE_ID "\n");
    else if (!strncmp(line, "SENDFILE ", 9)) return receive_file(c, line + 9) == 0;
    else if (!strcmp(line, "SENDFILE")) reply(c, "ERR 005 INVALID_FILE_HEADER " NODE_ID "\n");
    else if (!strncmp(line, "REGISTER", 8)) reply(c, "ERR 005 ALREADY_REGISTERED " NODE_ID "\n");
    else {
        log_event("ERROR", "Unknown command from %s", c->username);
        reply(c, "ERR 005 UNKNOWN_COMMAND " NODE_ID "\n");
    }
    return 1;
}
static void *handle_client(void *arg) {
    Client *c = arg;
    char line[MAX_LINE];
    for (;;) {
        int status = read_line(c->fd, line, sizeof line);
        if (status == 0) break;
        if (status == -1) { log_event("ERROR", "recv failed for %s: %s", c->registered ? c->username : "unregistered", strerror(errno)); break; }
        if (status == -2) {
            log_event("ERROR", "Command line too long from %s", c->registered ? c->username : "unregistered");
            reply(c, "ERR 005 LINE_TOO_LONG " NODE_ID "\n"); continue;
        }
        if (!process_command(c, line)) break;
    }
    pthread_mutex_lock(&lock);
    cleanup_rooms(c);
    if (c->registered) {
        notify_presence(c, "LEAVE");
        log_event("LEFT", "%s", c->username);
    }
    Client **p = &clients;
    while (*p && *p != c) p = &(*p)->next;
    if (*p) *p = c->next;
    --client_count;
    log_event("DISCONNECTED", "Active clients: %d", client_count);
    pthread_mutex_unlock(&lock);
    close(c->fd);
    free(c);
    return NULL;
}
int main(void) {
    signal(SIGPIPE, SIG_IGN);
    log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        fprintf(stderr, "Cannot open %s: %s\n", LOG_FILE, strerror(errno));
        return EXIT_FAILURE;
    }
    (void)setvbuf(log_file, NULL, _IOLBF, 0);
    int fd = socket(AF_INET, SOCK_STREAM, 0), opt = 1;
    if (fd < 0) { log_event("ERROR", "socket: %s", strerror(errno)); fclose(log_file); return EXIT_FAILURE; }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) != 0) {
        log_event("ERROR", "setsockopt: %s", strerror(errno)); close(fd); fclose(log_file); return EXIT_FAILURE;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(fd, 10) != 0) {
        log_event("ERROR", "bind/listen: %s", strerror(errno)); close(fd); fclose(log_file); return EXIT_FAILURE;
    }
    printf("====================================\n"
           "        NetMessenger Server\n"
           "====================================\n"
           "Listening on TCP port: %d\nNode ID: %s\n"
           "Concurrency Model: POSIX Threads\n"
           "Commands: REGISTER LIST BCAST PMSG\n"
           "          JOIN LEAVE ROOMS RMSG SENDFILE QUIT\n"
           "Maximum file size: 10 MiB\nLogging: %s\nWaiting for connections...\n"
           "====================================\n", PORT, NODE_ID, LOG_FILE);
    fflush(stdout);
    log_event("START", "Server listening on TCP port %d (%s)", PORT, NODE_ID);
    for (;;) {
        int peer = accept(fd, NULL, NULL);
        if (peer < 0) { if (errno != EINTR) log_event("ERROR", "accept: %s", strerror(errno)); continue; }
        Client *c = calloc(1, sizeof *c);
        if (!c) { log_event("ERROR", "Cannot allocate client"); close(peer); continue; }
        c->fd = peer;
        pthread_mutex_lock(&lock);
        if (client_count >= MAX_CLIENTS) {
            const char *msg = "ERR 005 SERVER_FULL " NODE_ID "\n";
            (void)send_all(peer, msg, strlen(msg));
            log_event("ERROR", "SERVER_FULL: rejected connection");
            pthread_mutex_unlock(&lock);
            close(peer); free(c); continue;
        }
        c->next = clients; clients = c;
        ++client_count;
        log_event("CONNECTED", "Active clients: %d", client_count);
        pthread_mutex_unlock(&lock);
        pthread_t thread;
        int rc = pthread_create(&thread, NULL, handle_client, c);
        if (rc) {
            log_event("ERROR", "pthread_create: %s", strerror(rc));
            pthread_mutex_lock(&lock);
            Client **p = &clients;
            while (*p && *p != c) p = &(*p)->next;
            if (*p) *p = c->next;
            --client_count;
            log_event("DISCONNECTED", "Active clients: %d (thread creation failed)", client_count);
            pthread_mutex_unlock(&lock);
            close(peer); free(c); continue;
        }
        rc = pthread_detach(thread);
        if (rc) log_event("ERROR", "pthread_detach: %s", strerror(rc));
    }
}
