# NetMessenger — AI Prompt Log

**Student ID:** IT23763562  
**Module:** IE3010 – Network Programming  
**AI Tool:** ChatGPT (OpenAI)

## Interaction 01 — Assignment Understanding

**Date:** 09 October 2026

**Prompt Summary:** Asked ChatGPT to explain the NetMessenger assignment requirements, submission instructions, and development process in simple Sinhala for a beginner.

**AI Assistance:** ChatGPT explained the required TCP client-server features, personalised values, GitHub requirements, documentation, testing evidence, and assessment components.

**How I Used the Output:** I used the explanations to understand the assignment requirements and organise the development process into manageable steps. I referred to the assignment document to check the required features.

## Interaction 02 — Project and Git Setup

**Date:** 09 October 2026

**Prompt Summary:** Requested step-by-step guidance for preparing the CentOS development environment and setting up Git and GitHub.

**AI Assistance:** ChatGPT provided Linux and Git commands for creating the project directory, preparing files, initializing the local repository, and configuring the GitHub remote.

**How I Used the Output:** I executed the commands in my CentOS terminal and checked their outputs. I also identified and corrected a mistake where I had entered the terminal prompt itself as a command.

## Interaction 03 — README and Design Diary

**Date:** 09 October 2026

**Prompt Summary:** Requested guidance for preparing the initial README and Design Diary according to the assignment requirements.

**AI Assistance:** ChatGPT suggested a README structure with personalised project values and drafted an initial Design Diary entry.

**How I Used the Output:** I added the content to my local project files and checked the saved text using terminal commands. I will update these documents as the actual implementation progresses.

---

**Note:** Additional AI interactions, especially code generation, debugging, testing, and report assistance, will be recorded throughout development. Full prompts or accurate prompt references should be retained where practical.

## Interaction 04 — Basic TCP Socket Programming

**Date:** 09 October 2026

**Prompt Summary:** Requested beginner-friendly guidance and complete C source code for creating and testing a basic TCP server and client on CentOS Linux.

**AI Assistance:** ChatGPT provided sample implementations using the BSD sockets API, explained the socket functions, and suggested compilation and connection-testing commands.

**How I Used the Output:** I copied the initial implementations into my personalised source files, compiled them using GCC, and tested the connections using Ncat and the client program. I checked the terminal outputs to confirm successful TCP connections. The current implementation only establishes and closes connections; the messaging functionality is still pending.

**Verification:** Both programs compiled without reported warnings, the server listened on port 9562, and the TCP connection tests succeeded.


## Interaction 05 — Multi-Client Concurrency

**Date:** 09 October 2026

**Prompt Summary:** Requested step-by-step guidance and complete updated C server code to support multiple simultaneous TCP clients using POSIX threads.

**AI Assistance:** ChatGPT provided a pthread-based server implementation with a separate thread for each client, mutex-protected client counting, connection handling, and disconnection detection. It also explained the threading functions and suggested tests using Ncat.

**How I Used the Output:** I replaced my previous server code with the updated implementation and compiled it using GCC with the `-pthread` option. I tested the server using five simultaneous Ncat connections and checked the server output as clients connected and disconnected.

**Testing Result:** The server displayed five active clients and correctly reduced the active-client count to zero after all clients disconnected. The tests completed without a reported server crash.

**Current Limitation:** This version handles TCP connections but does not yet implement the required chat commands, message forwarding, or file transfers.

**Learning Outcome:** I gained a better understanding of how threads allow multiple clients to remain connected and how a mutex protects shared data.

## Interaction 06 — User Registration and Presence

**Date:** 09 October 2026

**Prompt Summary:** Requested complete updated C server code and step-by-step testing instructions for user registration, listing connected users, duplicate username handling, and client disconnections.

**AI Assistance:** ChatGPT provided a server implementation using a mutex-protected user registry, line-based TCP command processing, personalised NID responses, and JOIN/LEAVE presence notifications. It also suggested tests using multiple Ncat clients.

**How I Used the Output:** I replaced the previous server code, compiled it using GCC, and tested REGISTER, LIST, duplicate usernames, QUIT, and unexpected disconnections. I compared the responses with the assignment protocol.

**Testing Result:** Registration and user listing worked. Duplicate usernames were rejected with the expected error response. QUIT returned OK BYE NID:7635, and disconnected users were removed from the active user list.

**Learning Outcome:** I learned how to manage registered users across multiple threads, validate usernames, process newline-terminated commands, and clean up user information after disconnection.

## Interaction 07 — Broadcast Messaging Implementation

**Date:** 09 October 2026

**Prompt Summary:** Requested complete updated C server code and step-by-step testing guidance for implementing broadcast messaging according to the NetMessenger protocol.

**AI Assistance:** ChatGPT provided an updated server implementation with the BCAST command, message forwarding to other registered clients, personalised NID responses, and handling for empty messages. It also suggested testing with three connected users.

**How I Used the Output:** I created a backup of my previous server code, replaced the existing implementation with the updated version, and compiled it using GCC with POSIX threads enabled. I tested broadcast messaging using three Ncat clients named amal, nimal, and kasun.

**Testing Result:** Messages sent by amal were delivered to nimal and kasun. A reverse broadcast from nimal also worked. The sender received OK SENT NID:7635, while recipients received correctly formatted MSG BCAST messages. Empty messages and invalid commands produced error responses. All clients disconnected successfully.

**Learning Outcome:** I learned how broadcast messaging is implemented using a shared client registry and how the server distinguishes sender responses from messages forwarded to other clients.

**Current Limitation:** Private messaging, chat rooms, file sharing, and server-side logging are not yet implemented.

## Interaction 08 — Private Messaging Implementation

**Date:** 09 October 2026

**Prompt Summary:** Requested complete updated C server code and step-by-step guidance to implement private messaging (PMSG) according to the NetMessenger assignment protocol, while preserving existing registration and broadcast functionality.

**AI Assistance:** ChatGPT provided an updated server implementation containing the find_user() and private_message() functions. It explained how the server identifies the target username, forwards a private message, sends a personalised response to the sender, and handles invalid or unknown usernames. It also suggested tests using three Ncat clients.

**How I Used the Output:** I backed up the existing server source, replaced it with the updated code, and compiled it with GCC using POSIX threads. I tested the PMSG command with three registered clients: amal, nimal, and kasun.

**Testing Result:** Private messages were delivered successfully between amal and nimal without being forwarded to kasun. Unknown usernames returned ERR 002 USER_NOT_FOUND NID:7635, and an incomplete PMSG command returned ERR 005 INVALID_PMSG NID:7635. I also verified that broadcast messaging continued to work and that clients could disconnect using QUIT.

**Learning Outcome:** I learned the difference between broadcast and private messaging, how to search for registered users in a shared client list, and why mutex synchronization is important in a multi-threaded server.

**Current Limitation:** Chat rooms, binary file transfer, server-side logging, and the complete interactive client application are still pending.


## Interaction 09 — Chat Room Management Implementation

**Date:** 09 October 2026

**Prompt Summary:** Requested complete updated C server code and step-by-step guidance for implementing JOIN, LEAVE, ROOMS, and RMSG commands while preserving existing NetMessenger functionality.

**AI Assistance:** ChatGPT provided an updated server implementation using linked lists to manage chat rooms and their memberships. It explained room creation, room listing, membership validation, room message forwarding, and automatic cleanup of empty rooms. It also provided testing instructions using multiple Ncat clients.

**How I Used the Output:** I replaced the existing server code with the updated implementation and compiled it using GCC with POSIX threads. I tested the room functionality using three registered clients named amal, nimal, and kasun.

**Testing Result:** Room creation and joining worked successfully. Messages were delivered only to the appropriate room members. Non-members and unknown rooms received ERR 003 ROOM_NOT_FOUND NID:7635. I also tested room leaving, automatic deletion of empty rooms, and unexpected client disconnections using Ctrl+C. All clients were eventually disconnected, and the server's active connection count returned to zero.

**Learning Outcome:** I learned how linked lists can represent multiple chat rooms and their members, how room membership controls message delivery, and how mutex synchronization helps manage shared information across client threads.

**Current Limitation:** Binary file transfer, persistent server-side logging, a complete interactive client application, and final documentation are still pending.


## Interaction 10 — Binary File Transfer Implementation

**Date:** 09 October 2026

**Prompt Summary:** Requested a complete updated C server implementation and step-by-step guidance to add the SENDFILE command while preserving existing NetMessenger functionalities.

**AI Assistance:** ChatGPT provided a complete updated server source file with binary file reception, personalised storage paths, a 10 MiB file size limit, filename validation, and interrupted-transfer cleanup. It also explained how to test raw binary data using Ncat and verify file integrity using SHA-256 and cmp.

**How I Used the Output:** I backed up my previous server source, copied the updated C file into my project, and compiled it using GCC with POSIX threads. I created a 30-byte binary test file and used Ncat to send the SENDFILE header followed by the exact raw bytes.

**Testing Result:** The server returned OK FILE_RECEIVED test_binary.bin NID:7635. The received file was stored under storage/IT23763562/amal/. SHA-256 hashes and cmp confirmed that the original and stored files were identical. An oversized file request returned ERR 004 FILE_TOO_LARGE, and an unsafe filename request returned ERR 005 INVALID_FILE_HEADER. I also tested a partial transfer by declaring 100 bytes but sending only five. The sender disconnected, and no incomplete file remained at the expected storage path.

**Learning Outcome:** I learned how TCP transports raw binary bytes, why file boundaries must be determined from the protocol header, and how file integrity and transfer errors can be tested.

**Current Limitation:** The file is stored on the server but is not automatically downloaded by the recipient. The complete interactive client, personalised Makefile, persistent logging, and final documentation are still pending.
