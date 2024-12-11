#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>


#define PORT 8080

// สร้าง struct เพื่อเก็บข้อมูล client
struct Client {
    int socket;
    std::string name;
};

// ประกาศ list ของ clients และ mutex เพื่อใช้ในการจัดการ thread
std::vector<Client> clients;
std::mutex mtx;
bool adminActive = false;

void handleClient(int client_socket, std::string client_name) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int valread = recv(client_socket, buffer, sizeof(buffer), 0);
        if (valread > 0) {
            // ส่งข้อความไปให้ทุก client ที่เชื่อมต่อ
            mtx.lock();
            for (auto& client : clients) {
                if (client.socket != client_socket) {
                    std::string msg = client_name + ": " + buffer;
                    send(client.socket, msg.c_str(), msg.length(), 0);
                }
            }
            mtx.unlock();
        } else {
            break;
        }
    }

    // เมื่อ client ออกจากเซิร์ฟเวอร์
    mtx.lock();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->socket == client_socket) {
            clients.erase(it);
            break;
        }
    }
    mtx.unlock();
}

void kickClient(const std::string& client_name) {
    mtx.lock();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->name == client_name) {
            std::string kick_msg = "You have been kicked out from the server.";
            send(it->socket, kick_msg.c_str(), kick_msg.length(), 0);
            close(it->socket); // ปิดการเชื่อมต่อของ client
            clients.erase(it);  // ลบ client ออกจาก vector
            std::cout << "Kicked out client: " << client_name << std::endl;
            break;
        }
    }
    mtx.unlock();
}


int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return -1;
    }

    std::cout << "Server is running on port " << PORT << "...\n";

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        return -1;
    }

    // การทำงานใน thread หลักที่รอรับคำสั่งจาก admin
    std::thread adminThread([&]() {
        while (true) {
            if (adminActive) {
                std::string command;
                std::cout << "Admin command: ";
                std::getline(std::cin, command);

                if (command.rfind("kick ", 0) == 0) {  // ถ้าคำสั่งคือ "kick <client_name>"
                    std::string client_name = command.substr(5);  // ดึงชื่อ client
                    kickClient(client_name);
                }
            } else {
                std::string adminCommand;
                std::cout << "Type 'admin' to activate admin commands.\n";
                std::getline(std::cin, adminCommand);

                if (adminCommand == "admin") {
                    adminActive = true;
                }
            }
        }
    });

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            return -1;
        }

        // รับชื่อของ client
        char buffer[1024] = {0};
        int valread = read(new_socket, buffer, sizeof(buffer));
        std::string client_name(buffer);

        // เพิ่ม client เข้าในรายการ clients
        mtx.lock();
        clients.push_back({new_socket, client_name});
        mtx.unlock();

        std::cout << "New client connected: " << client_name << std::endl;

        // สร้าง thread เพื่อรับข้อความจาก client
        std::thread(handleClient, new_socket, client_name).detach();
    }

    adminThread.join();

    return 0;
}
