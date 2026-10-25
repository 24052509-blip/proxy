# High-Performance Multi-Threaded HTTP Proxy Server in C

A robust, multi-threaded HTTP proxy server built from scratch in C using Windows Winsock (winsock2.h). This project is engineered to intercept, process, and forward client HTTP requests concurrently while implementing a high-speed memory caching mechanism to reduce redundant network latency.



##  Key Features

* *Multi-Threaded Architecture:* Utilizes Windows threads to handle multiple concurrent client connections simultaneously without blocking the main server listener.
* *In-Memory Response Caching:* Implements a custom dynamic cache structure (CacheNode) to store frequently accessed web resources, significantly cutting down external network overhead.
* *Robust Socket Programming:* Leverages low-level socket APIs for reliable TCP connection establishment, raw HTTP header parsing, and bi-directional payload tunneling.
* *Clean & Modular Design:* Structured for scalability, maintainability, and academic evaluation standards.



##  Tech Stack & Environment

* *Language:* C (C99 / C11 standard)
* *Networking API:* Windows Sockets (Winsock2)
* *Environment:* Windows OS, VS Code, GCC (MinGW)
* *Version Control:* Git & GitHub



## Project Structure


 CNPROXYSERVER.C       # Core source code (Socket, Multithreading & Caching logic)
 README.md             # Project documentation
 .gitignore            # Ignored build binaries and temporary files

##How to Compile and Run

Prerequisites
Make sure you have a C compiler like GCC (MinGW) installed on your Windows system.

1. Compilation

gcc CNPROXYSERVER.C -o proxy_server -lws2_32

2.Execution
./proxy_server

3. Client Configuration
Configure your web browser or testing tool (like curl or Postman) to route HTTP traffic through the local proxy server IP (127.0.0.1) and port (8080).



##TECH STACK AND DEPENDENCIES

Component        Specification

#Language          C (C99 / C11 Standard)

#Networking        Windows Sockets API (Winsock2.h, ws2_32.lib)
library 

#Platform           Microsoft Windows OS

#Development         ToolsVS Code, MinGW (GCC Compiler), Git


##ACADEMIC AND SYSTEM DESIGN CONCEPTS

This project directly demonstrates key concepts in system programming and networking:

Socket Programming: Non-blocking/blocking I/O, bind(), listen(), accept(), send(), and recv().

Operating Systems: Thread management, concurrency overhead, stack allocation per client thread, and memory management.

Computer Networks: HTTP/1.1 protocol architecture, header parsing, proxy tunneling, and latency optimization via caching.