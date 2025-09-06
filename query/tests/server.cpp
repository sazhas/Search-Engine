#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <string>

#include "../protocol_query.h"
#include "../../lib/networking.h"

#define PORT 9000

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    // Start listening for incoming connections
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    std::cout << "Server is listening on port " << PORT << "..." << std::endl;

    // Accept and handle client connections
    while (true) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        std::cout << "Client connected!" << std::endl;

        // Receive data from the client

        char window[2] = {0, 0};
        recv_looping_crashing(client_fd, window, 1);
        
        std::string request;
        request.reserve(512);

        int bytes_received;
        while (
            (bytes_received = recv(client_fd, window + 1, 1, 0)) 
            > 0
        ) {
            switch(window[0]) {
                case '\\':
                    request.push_back(window[1]);
                    recv_looping_crashing(client_fd, window, 1);
                    break;
                case Query::Protocol::QUERY_END:
                    std::cout << "Received: " << request << std::endl;
                    request.clear();
                    window[0] = window[1];
                    break;
                default:
                    request.push_back(window[0]);
                    window[0] = window[1];
                    break;
            }
            
        }

        if (bytes_received == 0) {
            std::cout << "Client disconnected." << std::endl;
        } else if (bytes_received < 0) {
            perror("Receive failed");
        }

        std::cout << "Received: " << request << std::endl;

        // Close the client connection
        close(client_fd);
    }

    // Close the server socket
    close(server_fd);
    return 0;
}