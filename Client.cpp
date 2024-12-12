#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <cstdlib>

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

    std::thread recvThread(receiveMessages, sock);

    while (true) {
        std::string message;
        std::getline(std::cin, message);
        if(message == "exit") exit(0);
        send(sock, message.c_str(), message.length(), 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    recvThread.join();  

    close(sock);
    return 0;
}
