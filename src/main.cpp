#include "server.h"
#include <iostream>
#include <sstream>
#include <csignal>

Server* global_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[INFO] Received signal " << signum << ", shutting down...\n";
    if (global_server) {
        global_server->shutdown();
    }
    exit(signum);
}

void printUsage(const char* prog_name) {
    std::cerr << "Usage:\n"
              << "  " << prog_name << " --id <id> --port <port> [--role <leader|follower>] [--peers p1,p2,...]\n"
              << "\n"
              << "Arguments:\n"
              << "  --id <id>           Unique server ID (integer)\n"
              << "  --port <port>       Port to listen on\n"
              << "  --role <role>       Initial role (leader or follower, default: follower)\n"
              << "  --peers <peers>     Comma-separated list of peer ports (e.g., 9001,9002)\n"
              << "\n"
              << "Example:\n"
              << "  # Start a 3-node cluster\n"
              << "  " << prog_name << " --id 1 --port 9001 --peers 9002,9003\n"
              << "  " << prog_name << " --id 2 --port 9002 --peers 9001,9003\n"
              << "  " << prog_name << " --id 3 --port 9003 --peers 9001,9002\n"
              << "\n";
}

int main(int argc, char* argv[]) {
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    if (argc < 5) {
        printUsage(argv[0]);
        return 1;
    }

    int id = -1;
    int port = -1;
    Role role = Role::FOLLOWER;
    std::vector<std::string> peers;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--id" && i + 1 < argc) {
            id = std::stoi(argv[++i]);
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--role" && i + 1 < argc) {
            std::string r = argv[++i];
            role = (r == "leader") ? Role::LEADER : Role::FOLLOWER;
        } else if (arg == "--peers" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string peer;
            while (std::getline(ss, peer, ',')) {
                peers.push_back("127.0.0.1:" + peer);
            }
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (id == -1 || port == -1) {
        std::cerr << "[ERROR] Missing required arguments: --id and --port\n\n";
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "LogKV - Distributed Key-Value Store\n";
    std::cout << "========================================\n";
    std::cout << "Server ID:   " << id << "\n";
    std::cout << "Port:        " << port << "\n";
    std::cout << "Initial Role:" << (role == Role::LEADER ? "LEADER" : "FOLLOWER") << "\n";
    std::cout << "Peers:       ";
    if (peers.empty()) {
        std::cout << "(none - single node)\n";
    } else {
        for (size_t i = 0; i < peers.size(); i++) {
            std::cout << peers[i];
            if (i < peers.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
    std::cout << "========================================\n\n";

    Server server(port, role, id, peers);
    global_server = &server;
    
    server.start();
    
    return 0;
}