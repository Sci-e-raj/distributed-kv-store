#include "server.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <thread>
#include <iostream>

Server::Server(int port)
    : port_(port), wal_("wal.log") {
    wal_.replay(store_);
}

void Server::start() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "LogKV running on port " << port_ << std::endl;

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        std::thread(&Server::handleClient, this, client).detach();
    }
}

void Server::handleClient(int client_fd) {
    char buffer[1024];
    int n = read(client_fd, buffer, sizeof(buffer));
    if (n <= 0) return;

    std::string req(buffer, n);
    std::istringstream iss(req);

    std::string cmd, key, value;
    iss >> cmd >> key;

    if (cmd == "PUT") {
        iss >> value;
        wal_.appendPut(key, value);
        store_.put(key, value);
        write(client_fd, "OK\n", 3);
    } else if (cmd == "GET") {
        if (store_.get(key, value)) {
            write(client_fd, value.c_str(), value.size());
            write(client_fd, "\n", 1);
        } else {
            write(client_fd, "NOT_FOUND\n", 10);
        }
    }

    close(client_fd);
}
