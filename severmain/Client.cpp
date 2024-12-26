#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <cstdlib>
#include <sstream>
#include <vector>

#define PORT 8080

void receiveMessages(int sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int valread = recv(sock, buffer, sizeof(buffer), 0);
        if (valread > 0) {
            std::cout << buffer << std::endl;
        }
        else if (valread == 0) {
            std::cout << "Server disconnected\n";
            break;
        } else {
            std::cout << "Error in receiving data\n";
            break;
        }
    }
}

void handleCommandInput(const std::string& input, int sock) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    if (tokens.empty()) return;
    
    std::string command = tokens[0];
    
    if (command == "help") {
        std::cout << "\n=== Available Commands ===\n"
                  << "c-create <room name> - Create a new chat room\n"
                  << "c-join <room name> - Join an existing chat room\n"
                  << "show -a - Show all available rooms\n"
                  << "clear - Clear the screen\n"
                  << "exit - Exit the program\n"
                  << "help - Show this help message\n\n";
    }
    else if (command == "clear") {
        system("clear");
    }
    else if (command == "c-create" && tokens.size() >= 2) {
        std::string msg = "CMD_CREATE_ROOM:" + tokens[1];
        send(sock, msg.c_str(), msg.length(), 0);
    }
    else if (command == "c-join" && tokens.size() >= 2) {
        std::string msg = "CMD_JOIN_ROOM:" + tokens[1];
        send(sock, msg.c_str(), msg.length(), 0);
    }
    else if (command == "show" && tokens.size() >= 2 && tokens[1] == "-a") {
        std::string msg = "CMD_SHOW_ROOMS";
        send(sock, msg.c_str(), msg.length(), 0);
    }
    else if (command == "exit") {
        exit(0);
    }
    else {
        // If not a command, treat as regular message
        send(sock, input.c_str(), input.length(), 0);
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "Socket creation failed\n";
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cout << "Invalid address or address not supported\n";
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "Connection failed\n";
        return -1;
    }

    std::cout << "Enter your name: ";
    std::string name;
    std::getline(std::cin, name);
    send(sock, name.c_str(), name.length(), 0);

    std::cout << "\nWelcome to the chat! Type 'help' to see available commands.\n\n";

    std::thread recvThread(receiveMessages, sock);

    while (true) {
        std::string input;
        std::getline(std::cin, input);
        handleCommandInput(input, sock);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    recvThread.join();
    close(sock);
    return 0;
}