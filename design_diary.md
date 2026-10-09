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
