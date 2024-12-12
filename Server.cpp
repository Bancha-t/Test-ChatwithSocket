#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <cstdlib>

#define PORT 8080

struct Client {
    int socket;
    std::string name;
};

std::vector<Client> clients;
std::vector<std::string> chatHistory;
std::mutex mtx;
bool adminActive = false;

void handleClient(int client_socket, std::string client_name) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int valread = recv(client_socket, buffer, sizeof(buffer), 0);
        if (valread > 0) {
            std::string msg = client_name + ": " + buffer;

            mtx.lock();
            chatHistory.push_back(msg);

            for (const auto& client : clients) {
                if (client.socket != client_socket) {
                    send(client.socket, msg.c_str(), msg.length(), 0);
                }
            }
            mtx.unlock();
        } else {
            break;
        }
    }

    mtx.lock();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->socket == client_socket) {
            std::string disconnect_msg = it->name + " has left the chat.";
            
            for (const auto& client : clients) {
                if (client.socket != client_socket) {
                    send(client.socket, disconnect_msg.c_str(), disconnect_msg.length(), 0);
                }
            }
            
            clients.erase(it);
            break;
        }
    }
    mtx.unlock();


    close(client_socket);
    std::cout << client_name << " disconnected.\n";
}

void kickClient(const std::string& client_name) {
    mtx.lock();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->name == client_name) {
            std::string kick_msg = "You have been kicked out from the server.";
            send(it->socket, kick_msg.c_str(), kick_msg.length(), 0);
            close(it->socket);
            clients.erase(it);
            std::cout << "Kicked out client: " << client_name << std::endl;
            break;
        }
    }
    mtx.unlock();
}

void handleInputCommand() {
    while (true) {
        std::string command;
        std::getline(std::cin, command);

        std::istringstream iss(command);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        if (tokens.empty()) continue;
        if (tokens[0] == "clear") {
            system("clear");
            std::cout << "Server is running on port " << PORT << "...\n";
        }
        if (tokens[0] == "show") {
            if (tokens.size() >= 2) {
                if (tokens[1] == "-u") {
                    mtx.lock();
                    std::cout << "=== Connected Users ===" << std::endl;
                    for (const auto& client : clients) {
                        std::cout << "  " << client.name << std::endl;
                    }
                    mtx.unlock();
                } else if (tokens[1] == "-c") {
                    mtx.lock();
                    std::cout << "=== Chat History ===" << std::endl;
                    for (const auto& msg : chatHistory) {
                        std::cout << "  " << msg << std::endl;
                    }
                    mtx.unlock();
                } else {
                    std::cout << "Unknown option for show command. Use -u (users) or -c (chat).\n";
                }
            } else {
                std::cout << "Usage: show -u | show -c\n";
            }
        } 
        else if (tokens[0] == "kick" && tokens.size() >= 2) {
            kickClient(tokens[1]);
        }
        else if (tokens[0] == "close" && tokens.size() >= 2 && tokens[1] == "-s") {
            std::cout << "Server shutting down...\n";

            mtx.lock();
            for (auto& client : clients) {
                std::string shutdown_msg = "Server is shutting down.";
                send(client.socket, shutdown_msg.c_str(), shutdown_msg.length(), 0);
                close(client.socket);
            }
            clients.clear();
            mtx.unlock();

        exit(0); 
    }
    else continue;
    }
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

    std::thread adminThread(handleInputCommand);

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            return -1;
        }

        char buffer[1024] = {0};
        int valread = read(new_socket, buffer, sizeof(buffer));
        std::string client_name(buffer);

        mtx.lock();
        clients.push_back({new_socket, client_name});
        mtx.unlock();

        std::cout << "New client connected: " << client_name << std::endl;

        std::thread(handleClient, new_socket, client_name).detach();
    }

    adminThread.join();
    return 0;
}