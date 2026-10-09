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
