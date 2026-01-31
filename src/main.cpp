#include "server.h"
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage:\n"
                  << "./logkv --id <id> --port <port> --role <leader|follower> --peers p1,p2,...\n";
        return 1;
    }

    int id = -1;
    int port = -1;
    Role role = Role::FOLLOWER;
    std::vector<std::string> peers;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--id") {
            id = std::stoi(argv[++i]);
        } else if (arg == "--port") {
            port = std::stoi(argv[++i]);
        } else if (arg == "--role") {
            std::string r = argv[++i];
            role = (r == "leader") ? Role::LEADER : Role::FOLLOWER;
        } else if (arg == "--peers") {
            std::stringstream ss(argv[++i]);
            std::string peer;
            while (std::getline(ss, peer, ',')) {
                peers.push_back("127.0.0.1:" + peer);
            }
        }
    }

    if (id == -1 || port == -1 || peers.empty()) {
        std::cerr << "Invalid arguments\n";
        return 1;
    }

    Server server(port, role, id, peers);
    server.start();
    return 0;
}