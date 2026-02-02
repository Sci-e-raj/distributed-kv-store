#pragma once
#include "snapshot.h"

class SnapshotManager {
public:
    SnapshotManager(const std::string& path)
        : path_(path) {}

    void save(const Snapshot& snap);
    bool load(Snapshot& snap);

private:
    std::string path_;
};
