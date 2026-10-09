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
