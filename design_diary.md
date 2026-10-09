# NetMessenger — Design Diary

**Student ID:** IT23763562  
**Module:** IE3010 – Network Programming

### Entry 01 — 09 October 2026
**Activity:** Project Initialization and Git Setup

I started the NetMessenger assignment by preparing my project environment in CentOS Linux. I created the required C source files, personalised Makefile, README, design diary, and AI prompt log.

I calculated the personalised values using my registration number: port 9562 and Node ID NID:7635. I also initialized a local Git repository, configured the GitHub remote, and prepared the initial README with the project details and planned features.

**Design Decision:** I decided to organise the project using the exact personalised filenames specified in the assignment. I will develop and test the required features step by step before adding them to the Git commit history.

**Challenge:** I initially entered the terminal prompt itself as a command and received an error. After identifying the mistake, I continued using only the required Linux commands.

**Learning Outcome:** I improved my understanding of basic Linux commands, Git repository initialization, and the importance of project organisation before starting socket programming.


### Entry 02 — 09 October 2026
**Activity:** Basic TCP Server and Client Implementation

I implemented the initial TCP server and client programs in C using the BSD socket API. I configured the server to use my personalised port 9562 and tested the connection using Ncat before writing and testing the client program.

The server successfully accepted TCP connections, and the client connected using the localhost address. I compiled both programs with GCC and did not receive any compiler warnings.

**Design Decision:** I started with a basic TCP connection before implementing the messaging protocol. This allowed me to test the network connection separately from the more advanced features.

**Challenge:** The initial server terminated after starting because it did not yet have a connection-handling loop. I updated the server to continuously accept connections and verified that it was listening using the `ss` command.

**Learning Outcome:** I learned how `socket()`, `bind()`, `listen()`, `accept()`, and `connect()` work together in a TCP client-server application.


### Entry 03 — 09 October 2026
**Activity:** Multi-Client Concurrency Implementation

I implemented multi-client connection handling using POSIX threads. Each accepted client connection is handled by a separate thread, while a mutex protects the shared active-client counter.

I tested the server by opening five Ncat connections simultaneously. The server correctly displayed five active clients. I then disconnected the clients one by one and verified that the active-client count returned to zero.

**Design Decision:** I selected POSIX threads because they allow the server to handle multiple clients concurrently while keeping a separate execution path for each connection.

**Testing Result:** The server successfully maintained five simultaneous TCP connections and handled individual disconnections without crashing during the test.

**Learning Outcome:** I learned how `pthread_create()`, `pthread_detach()`, and mutex locking help manage multiple TCP clients.


### Entry 04 — 09 October 2026
**Activity:** User Registration, Presence and Disconnection Handling

I implemented user registration and listing using the specified line-based TCP protocol. I added a shared user list protected by a mutex to prevent duplicate usernames when multiple clients register concurrently.

I tested registration with different usernames, duplicate username detection, and the LIST command. I also tested graceful disconnection using QUIT and unexpected disconnection by terminating an Ncat client with Ctrl+C.

**Design Decision:** I used a mutex-protected user registry to manage connected users safely across multiple client threads.

**Testing Result:** The server returned the correct personalised responses using NID:7635. JOIN and LEAVE notifications worked, and disconnected users were removed from the active user list.

**Learning Outcome:** I learned how a multi-threaded server maintains shared user information and detects client disconnections.


### Entry 05 — 09 October 2026
**Activity:** Broadcast Messaging Implementation

I implemented the BCAST command using the existing multi-threaded server. The server forwards a broadcast message to all other registered clients while sending an OK SENT response with my personalised NID tag to the sender.

I tested the functionality using three connected users: amal, nimal, and kasun. Messages from amal were delivered to nimal and kasun, and a reverse broadcast from nimal was also successful.

**Design Decision:** I used the shared client list and mutex to coordinate message delivery between registered clients.

**Testing Result:** Broadcast messaging worked in both directions. Empty messages and unknown commands returned error responses without crashing the server. All clients disconnected successfully, and the active connection count returned to zero.

**Learning Outcome:** I learned how the server forwards messages between multiple TCP clients and why concurrent access to shared connection data must be controlled.


### Entry 06 — 09 October 2026
**Activity:** Private Messaging Implementation

I added the PMSG command to the existing multi-threaded TCP server. The server searches for the requested username and forwards the message only to the matching registered client. The sender receives an OK SENT response containing my personalised NID tag.

**Design Decision:** I reused the shared client list and mutex for private message delivery. This allows the server to locate the recipient while protecting shared connection information.

**Testing Result:** I tested private messaging using amal, nimal, and kasun. Messages sent between amal and nimal were delivered correctly, while kasun did not receive those private messages. An unknown username returned ERR 002 USER_NOT_FOUND, and an incomplete PMSG command returned an error. I also confirmed that broadcast messaging continued to work and that all three clients responded correctly to QUIT.

**Learning Outcome:** I learned how private messaging differs from broadcasting and how to locate a specific connected user safely in a multi-threaded TCP server.


### Entry 07 — 09 October 2026
**Activity:** Chat Room Management Implementation

I implemented the JOIN, LEAVE, ROOMS, and RMSG commands in my NetMessenger TCP server. I used linked lists to maintain chat rooms and their members, with a POSIX mutex to protect shared information.

**Design Decision:** A room is created when the first user joins it and removed automatically when its last member leaves. Room messages are forwarded only to other registered members of the same room.

**Testing Result:** I tested the implementation using three clients: amal, nimal, and kasun. I confirmed room creation, membership validation, room message delivery, and correct error responses for non-members and unknown rooms. I also tested abrupt client disconnections using Ctrl+C. The server removed disconnected users from their rooms, and empty rooms were deleted automatically. All clients were eventually disconnected, and the active connection count returned to zero.

**Learning Outcome:** I learned how to manage room memberships using linked data structures, how to restrict messages to specific groups, and why shared room information requires thread synchronization.


### Entry 08 — 09 October 2026
**Activity:** Binary File Transfer Implementation

I implemented the SENDFILE command in the NetMessenger server to receive binary files over TCP and save them in the personalised storage directory.

**Design Decision:** The server reads the file header first and then receives the exact number of raw bytes specified by the sender. I used a maximum file size of 10 MiB and included filename validation and incomplete-transfer cleanup.

**Testing Result:** I transferred a 30-byte binary test file containing newline, null, and non-ASCII bytes. The stored file had the same size and SHA-256 hash as the original, and the cmp command confirmed that both files were identical. Oversized file requests and unsafe filenames were rejected. An interrupted transfer did not leave an incomplete file at the expected storage path. All clients disconnected, and the active connection count returned to zero.

**Learning Outcome:** I learned the difference between text-based commands and raw binary data in a TCP stream, how to verify file integrity, and why file-size validation and cleanup are important.
