Network Chat Program

This is a straightforward network messaging application developed in C++ utilizing socket programming. It consists of a server and several clients that are able to chat in real time.

## How to Compile

### Step 1: Compile the Server

Having obtained the `Server.cpp` and `Client.cpp` files, you need to generate both the server and client programs by using the following commands:
```bash
g++ Server.cpp -o server -pthread
g++ Client.cpp -o client -pthread
```
### Step 2: Run the Program

fter compiling the programs, the next step is to run them as follows: 
  1. Run the server: Open one terminal and type in the prompt below to launch the server:
```bash
./server
```
  2. Run the client: Open another terminal and execute the following command to start the client:
```bash
./client
```
###version 0.0.0 Add a Server and Client for chat.
###version 0.0.1 Add a function to create a room and prepare for the attack. ?? There is a problem with creating the room.