#include "server.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <thread>
#include <iostream>

void Server::startElection() {
    current_term_++;
    voted_for_ = server_id_;

    int votes = 1; // vote for self

    for (const auto& addr : peers_){
        std::string ip = addr.substr(0, addr.find(':'));
        int port = std::stoi(addr.substr(addr.find(':') + 1));

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        sockaddr_in serv{};
        serv.sin_family = AF_INET;
        serv.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serv.sin_addr);

        if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) {
            close(sock);
            continue;
        }

        std::ostringstream oss;
        oss << "REQUEST_VOTE " << current_term_ << " " << server_id_ << "\n";
        std::string msg = oss.str();

        write(sock, msg.c_str(), msg.size());

        char buffer[128];
        int n = read(sock, buffer, sizeof(buffer));
        if (n > 0 && std::string(buffer, n).find("VOTE_GRANTED") != std::string::npos) {
            votes++;
        }

        close(sock);
    }

    if (votes > 1) {
        role_ = Role::LEADER;
        std::cout << "[INFO] Became LEADER for term " << current_term_ << "\n";
        startHeartbeatSender();
    }
}

void Server::startHeartbeatMonitor() {
    last_heartbeat_ = std::chrono::steady_clock::now();

    std::thread([this]() {
        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_heartbeat_
            ).count();

            // if (diff > 3) {
            //     leader_alive_ = false;
            //     std::cout << "[WARN] Leader considered dead\n";
            // }
            if (diff > 3 && role_ == Role::FOLLOWER) {
                leader_alive_ = false;
                std::cout << "[WARN] Leader dead. Starting election...\n";
                startElection();
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();
}

void Server::startHeartbeatSender() {
    std::thread([this]() {
        while (true) {
            if (replicator_) {
                replicator_->sendHeartbeats();
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();
}

Server::Server(int port, Role role, int server_id,
               const std::vector<std::string>& peers)
    : port_(port),
      role_(role),
      server_id_(server_id),
      peers_(peers),
      wal_("wal_" + std::to_string(port) + ".log") {

    wal_.replay(store_);

    if (role_ == Role::LEADER) {
        replicator_ = std::make_unique<Replicator>(peers_);
    }
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

    if (role_ == Role::LEADER) {
        startHeartbeatSender();
    }
    if (role_ == Role::FOLLOWER) {
        startHeartbeatMonitor();
    }

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        std::thread(&Server::handleClient, this, client).detach();
    }
}

void Server::handleClient(int client_fd) {
    char buffer[1024];
    int n = read(client_fd, buffer, sizeof(buffer));
    if (n <= 0) {
        close(client_fd);
        return;
    }

    std::string req(buffer, n);
    std::istringstream iss(req);

    std::string cmd;
    iss >> cmd;

    if (cmd == "REQUEST_VOTE") {
        int term, candidate_id;
        iss >> term >> candidate_id;

        if (term > current_term_) {
            current_term_ = term;
            voted_for_ = -1;
        }

        if (voted_for_ == -1 && term == current_term_) {
            voted_for_ = candidate_id;
            write(client_fd, "VOTE_GRANTED\n", 13);
        } else {
            write(client_fd, "VOTE_DENIED\n", 12);
        }

        close(client_fd);
        return;
    }

    if (cmd == "HEARTBEAT") {
        last_heartbeat_ = std::chrono::steady_clock::now();
        leader_alive_ = true;

        write(client_fd, "OK\n", 3);
        close(client_fd);
        return;
    }

    // ---------- REPLICATION MESSAGE ----------
    if (cmd == "REPL_PUT") {
        std::string key, value;
        iss >> key >> value;

        wal_.appendPut(key, value);
        store_.put(key, value);

        write(client_fd, "ACK\n", 4);
        close(client_fd);
        return;
    }

    // ---------- CLIENT PUT ----------
    if (cmd == "PUT") {
        std::string key, value;
        iss >> key >> value;

        if (role_ == Role::FOLLOWER) {
            write(client_fd, "NOT_LEADER\n", 11);
            close(client_fd);
            return;
        }

        wal_.appendPut(key, value);
        store_.put(key, value);

        if (replicator_) {
            replicator_->replicatePut(key, value);
        }

        write(client_fd, "OK\n", 3);
        close(client_fd);
        return;
    }

    // ---------- CLIENT GET ----------
    if (cmd == "GET") {
        std::string key, value;
        iss >> key;

        if (store_.get(key, value)) {
            write(client_fd, value.c_str(), value.size());
            write(client_fd, "\n", 1);
        } else {
            write(client_fd, "NOT_FOUND\n", 10);
        }

        close(client_fd);
        return;
    }

    // ---------- UNKNOWN ----------
    write(client_fd, "UNKNOWN_CMD\n", 12);
    close(client_fd);
}
