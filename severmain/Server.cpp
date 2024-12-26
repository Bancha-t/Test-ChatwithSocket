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
#include <algorithm>

#define PORT 8080

struct Client {
    int socket;
    std::string name;
    std::string chatRoom;
};

struct ChatRoom {
    std::string name;
    std::vector<Client*> members;
};

std::vector<Client> clients;
std::vector<ChatRoom> chatRooms;
std::vector<std::string> chatHistory;
std::mutex mtx;

void broadcastToRoom(const std::string& message, const std::string& roomName, int excludeSocket = -1) {
    mtx.lock();
    for (auto& room : chatRooms) {
        if (room.name == roomName) {
            for (auto member : room.members) {
                if (member->socket != excludeSocket) {
                    send(member->socket, message.c_str(), message.length(), 0);
                }
            }
            break;
        }
    }
    mtx.unlock();
}

void createRoom(const std::string& roomName, Client* creator) {
    std::cout << "Debug: Attempting to create room: " << roomName << std::endl; // Debug line
    
    mtx.lock();
    // Check if room already exists
    for (const auto& room : chatRooms) {
        if (room.name == roomName) {
            std::string error_msg = "Room '" + roomName + "' already exists.\n";
            send(creator->socket, error_msg.c_str(), error_msg.length(), 0);
            mtx.unlock();
            std::cout << "Debug: Room creation failed - already exists" << std::endl; // Debug line
            return;
        }
    }
    
    // Create new room
    ChatRoom newRoom{roomName, {creator}};
    chatRooms.push_back(newRoom);
    creator->chatRoom = roomName;
    
    std::string success_msg = "Created and joined room: " + roomName + "\n";
    send(creator->socket, success_msg.c_str(), success_msg.length(), 0);
    std::cout << "Debug: Room successfully created: " << roomName << std::endl; // Debug line
    mtx.unlock();
}

void joinRoom(const std::string& roomName, Client* client) {
    mtx.lock();
    for (auto& room : chatRooms) {
        if (room.name == roomName) {
            // Leave current room if any
            if (!client->chatRoom.empty()) {
                for (auto& oldRoom : chatRooms) {
                    if (oldRoom.name == client->chatRoom) {
                        auto it = std::find_if(oldRoom.members.begin(), oldRoom.members.end(),
                            [client](Client* c) { return c->socket == client->socket; });
                        if (it != oldRoom.members.end()) {
                            oldRoom.members.erase(it);
                        }
                    }
                }
            }
            
            room.members.push_back(client);
            client->chatRoom = roomName;
            
            std::string join_msg = client->name + " has joined the room.";
            broadcastToRoom(join_msg, roomName);
            
            std::string success_msg = "Joined room: " + roomName;
            send(client->socket, success_msg.c_str(), success_msg.length(), 0);
            mtx.unlock();
            return;
        }
    }
    
    std::string error_msg = "Room '" + roomName + "' does not exist.";
    send(client->socket, error_msg.c_str(), error_msg.length(), 0);
    mtx.unlock();
}

void handleClient(int client_socket, std::string client_name) {
    char buffer[1024];
    Client* currentClient = nullptr;
    
    mtx.lock();
    for (auto& client : clients) {
        if (client.socket == client_socket) {
            currentClient = &client;
            break;
        }
    }
    mtx.unlock();
    
    if (!currentClient) return;
    
    std::string join_msg = client_name + " has joined the chat.";
    mtx.lock();
    for (const auto& client : clients) {
        if (client.socket != client_socket) {
            send(client.socket, join_msg.c_str(), join_msg.length(), 0);
        }
    }
    mtx.unlock();

        while (true) {
        memset(buffer, 0, sizeof(buffer));
        int valread = recv(client_socket, buffer, sizeof(buffer), 0);
        if (valread > 0) {
            std::string message(buffer);
            std::cout << "Debug: Received message: " << message << std::endl; // Debug line
            
            if (message.substr(0, 15) == "CMD_CREATE_ROOM:") {
                std::string roomName = message.substr(15);
                std::cout << "Debug: Creating room: " << roomName << std::endl; // Debug line
                createRoom(roomName, currentClient);
            }
            else if (message.substr(0, 13) == "CMD_JOIN_ROOM:") {
                std::string roomName = message.substr(13);
                std::cout << "Debug: Joining room: " << roomName << std::endl; // Debug line
                joinRoom(roomName, currentClient);
            }
            else if (message == "CMD_SHOW_ROOMS") {
                mtx.lock();
                std::string rooms_list = "\n=== Available Rooms ===\n";
                for (const auto& room : chatRooms) {
                    rooms_list += room.name + " <" + 
                                 std::to_string(room.members.size()) + " users>\n";
                }
                if (chatRooms.empty()) {
                    rooms_list += "No rooms available.\n"; // Add this line
                }
                rooms_list += "\n";  // Add extra newline
                send(client_socket, rooms_list.c_str(), rooms_list.length(), 0);
                mtx.unlock();
            }
            else if (!currentClient->chatRoom.empty()) {
                // Regular message in a room
                std::string msg = client_name + ": " + message;
                broadcastToRoom(msg, currentClient->chatRoom, client_socket);
                chatHistory.push_back(msg);
            }
            else {
                // User not in any room, send message
                std::string error_msg = "You are not in any room. Please create or join a room first.\n";
                send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            }
        } else {
            break;
        }
    }

    // Handle disconnection
    mtx.lock();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->socket == client_socket) {
            std::string disconnect_msg = it->name + " has left the chat.";
            
            if (!it->chatRoom.empty()) {
                for (auto& room : chatRooms) {
                    if (room.name == it->chatRoom) {
                        auto member_it = std::find_if(room.members.begin(), room.members.end(),
                            [&it](Client* c) { return c->socket == it->socket; });
                        if (member_it != room.members.end()) {
                            room.members.erase(member_it);
                        }
                    }
                }
            }
            
            // Notify other clients
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

        if (tokens[0] == "help") {
            std::cout << "\n=== Server Commands ===\n"
                      << "show -u : Show all connected users\n"
                      << "show -c : Show chat history\n"
                      << "show -a : Show all chat rooms\n"
                      << "kick <username> : Kick a user\n"
                      << "clear : Clear the screen\n"
                      << "close -s : Shut down the server\n"
                      << "help : Show this help message\n\n";
        }
        else if (tokens[0] == "clear") {
            system("clear");
            std::cout << "Server is running on port " << PORT << "...\n";
        }
        else if (tokens[0] == "show") {
            if (tokens.size() >= 2) {
                if (tokens[1] == "-u") {
                    mtx.lock();
                    std::cout << "\n=== Connected Users ===\n";
                    for (const auto& client : clients) {
                        std::cout << "  " << client.name 
                                << (client.chatRoom.empty() ? "" : " (in room: " + client.chatRoom + ")")
                                << std::endl;
                    }
                    mtx.unlock();
                }
                else if (tokens[1] == "-c") {
                    mtx.lock();
                    std::cout << "\n=== Chat History ===\n";
                    for (const auto& msg : chatHistory) {
                        std::cout << "  " << msg << std::endl;
                    }
                    mtx.unlock();
                }
                else if (tokens[1] == "-a") {
                    mtx.lock();
                    std::cout << "\n=== Chat Rooms ===\n";
                    for (const auto& room : chatRooms) {
                        std::cout << "  " << room.name << " <" 
                                << room.members.size() << " users>" << std::endl;
                    }
                    mtx.unlock();
                }
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
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return -1;
    }

    // Set socket options to reuse address and port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        return -1;
    }

    // Setup server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return -1;
    }

    std::cout << "Server is running on port " << PORT << "...\n";
    std::cout << "Type 'help' to see available commands.\n\n";

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        return -1;
    }

    // Start admin command handling thread
    std::thread adminThread(handleInputCommand);
    adminThread.detach();

    // Accept and handle client connections
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Receive client's name
        char buffer[1024] = {0};
        int valread = read(new_socket, buffer, sizeof(buffer));
        std::string client_name(buffer);

        // Add new client to clients vector
        mtx.lock();
        clients.push_back({new_socket, client_name, ""});
        mtx.unlock();

        std::cout << "New client connected: " << client_name << std::endl;

        // Create and detach thread for handling this client
        std::thread clientThread(handleClient, new_socket, client_name);
        clientThread.detach();
    }

    return 0;
}