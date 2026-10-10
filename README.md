# NetMessenger — Multi-Client Chat and File-Sharing Platform

## Student Information

- **Registration Number:** IT23763562
- **Module:** IE3010 — Network Programming
- **Project:** NetMessenger
- **Programming Language:** C
- **Operating System:** CentOS Linux

## Project Overview

NetMessenger is a TCP-based client-server communication application implemented in C using BSD sockets and POSIX threads. It supports concurrent client connections, unique username registration, user presence notifications, broadcast and private messaging, chat room management, binary file uploads, and timestamped server-side logging.

## Personalisation Details

| Item | Value |
|---|---|
| Registration Number | IT23763562 |
| Last Four Digits | 3562 |
| Server Port | 6000 + 3562 = 9562 |
| Node ID | NID:7635 |
| Server Source | server_3562.c |
| Client Source | client_3562.c |
| Makefile | Makefile_3562 |
| Log File | netmsg_IT23763562.log |
| File Storage | storage/IT23763562/<sender_username>/<filename> |
| Submission Archive | IE3010_IT23763562.zip |

## Implemented Features

- Multiclient TCP server using POSIX threads
- Unique username registration and active user listing
- User join and leave notifications
- Broadcast and private messaging
- Chat room creation, joining, leaving, listing, and messaging
- Binary file uploads through the server
- Server-side storage under the personalised directory
- File-size validation (maximum 10 MiB)
- Filename validation and interrupted-transfer cleanup
- Error responses and client disconnection handling
- Persistent timestamped event logging

## Build Instructions

Open a terminal in the project directory.

```bash
make -f Makefile_3562
```

This command builds the following executables:

- `server_3562`
- `client_3562`

Remove executables using:

```bash
make -f Makefile_3562 clean
```

## Running the Application

### Start the Server

Open Terminal 1:

```bash
make -f Makefile_3562 run-server
```

The server listens on TCP port `9562`.

### Start a Client

Open Terminal 2:

```bash
make -f Makefile_3562 run-client
```

Register a username:

```text
REGISTER amal
```

Start additional clients in separate terminals and register different usernames.

## Supported Commands

| Command | Description |
|---|---|
| `REGISTER <username>` | Register a unique username |
| `LIST` | Display active users |
| `BCAST <message>` | Send a broadcast message |
| `PMSG <username> <message>` | Send a private message |
| `JOIN <room>` | Create or join a chat room |
| `LEAVE <room>` | Leave a chat room |
| `ROOMS` | List active chat rooms |
| `RMSG <room> <message>` | Send a room message |
| `SENDFILE <target> <local_file_path>` | Upload a file using the interactive client |
| `QUIT` | Disconnect gracefully |

For the wire protocol, the client sends a `SENDFILE` header containing the target, filename and file size, followed by the raw file bytes.

## File Transfer

The server accepts binary file uploads of up to 10 MiB and stores successfully received files under:

```text
storage/IT23763562/<sender_username>/<filename>
```

File integrity was tested using SHA-256 hashes and the `cmp` utility.

**File Delivery:** NetMessenger supports binary file uploads to the server and forwarding to the intended recipient client. Received files are saved in the client's `received_files/` directory. Direct file delivery from `amal` to `nimal` was successfully verified using a 30-byte binary test file. Matching SHA-256 hashes and a successful byte-by-byte `cmp` comparison confirmed the integrity of the delivered file. Chat-room file delivery and additional edge cases require further verification.
## Server Logging

The server records timestamped connection, registration, messaging, room activity, file transfer, error, and disconnection events in:

```text
netmsg_IT23763562.log
```

View recent records using:

```bash
tail -n 20 netmsg_IT23763562.log
```

## Testing Summary

The implementation was tested on CentOS Linux using GCC.

Verified tests include:

- Five simultaneous TCP client connections
- Unique username registration and duplicate username rejection
- User listing and presence notifications
- Broadcast and private messaging
- Chat room messaging and membership validation
- Graceful and unexpected client disconnection
- Binary upload integrity using SHA-256 and `cmp`
- File-size limit and unsafe filename rejection
- Interrupted file transfer cleanup
- Timestamped event logging

## Project Documentation

- `design_diary.md` — Development decisions and challenges
- `prompt_log.md` — Record of AI assistance
- `reflection.md` — Structured learning reflection
- Implementation Report — Architecture, testing results, and screenshot evidence

## Version Control

The project was developed incrementally using Git and GitHub.

Repository:

https://github.com/fernandowvjtn/IE3010_IT23763562

## Conclusion

NetMessenger demonstrates TCP socket programming, concurrent client handling, message routing, basic chat room management, server-side binary file storage, error handling, and persistent logging in C.
