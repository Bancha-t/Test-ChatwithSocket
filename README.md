# Network Chat Program

This is a simple network chat program written in C++ using socket programming. It consists of a server and multiple clients that can communicate with each other in real-time.

## How to Compile

### Step 1: Compile the Server

Once you have the `Server.cpp` and `Client.cpp` files, compile both the server and client programs with the following commands:
```bash
g++ Server.cpp -o server -pthread
g++ Client.cpp -o client -pthread

### Step 2: Run the Program
