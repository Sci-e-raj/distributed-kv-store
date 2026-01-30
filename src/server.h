#pragma once
#include "store.h"
#include "wal.h"

class Server {
public:
    Server(int port);
    void start();

private:
    int port_;
    KVStore store_;
    WriteAheadLog wal_;

    void handleClient(int client_fd);
};
