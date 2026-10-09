# Structured Reflection

**Module:** IE3010 – Network Programming  
**Assignment:** NetMessenger  
**Student ID:** IT23763562

### 1. Development Approach

The NetMessenger project provided an opportunity to apply network programming concepts through the development of a multi-client TCP communication system using C on CentOS Linux. The implementation involved socket communication, concurrent client handling, user registration, message routing, chat room management, binary file transfer, and server-side logging.

The project was developed incrementally, with each major component compiled and tested before progressing to the next stage. Git was used to maintain version history and track implementation milestones.

### 2. Use of AI Assistance

ChatGPT was used as a supplementary technical resource during the development process. Its assistance included clarifying selected networking concepts, suggesting implementation approaches, reviewing C programming logic, and supporting debugging and testing activities.

AI-generated code suggestions were also used during the implementation of selected components, particularly where concurrent communication and binary data handling required additional attention. These suggestions were reviewed against the assignment requirements and tested in the CentOS environment before being incorporated into the project.

The relevant AI interactions and their application were documented separately in the AI Prompt Log.

### 3. Evaluation and Verification

A key consideration throughout development was ensuring that the implementation behaved according to the required communication protocol rather than relying solely on suggested solutions.

The server and client were compiled using GCC with appropriate warning flags. Functional tests covered user registration, duplicate username handling, broadcast and private messaging, chat room operations, and client disconnection.

Concurrent operation was verified using five simultaneously connected clients. File transfer reliability was evaluated using a binary test file, with SHA-256 hashing and byte-level comparison confirming that the server-stored file matched the original.

Additional testing addressed invalid filenames, file-size restrictions, interrupted transfers, and timestamped event logging. Environment-specific issues were resolved through direct testing and adjustments to the commands used.

### 4. Challenges and Learning Outcomes

The most significant technical challenges involved managing multiple client connections, maintaining shared application state, and distinguishing command messages from raw binary data in a TCP stream.

Implementing thread synchronization, validating protocol inputs, and handling unexpected disconnections strengthened my understanding of reliable client-server application design.

The project also highlighted the importance of structured testing, accurate documentation, and incremental version control.

Overall, the assignment improved my practical understanding of TCP socket programming and reinforced the value of critically evaluating AI-assisted suggestions through compilation, testing, and independent verification.
