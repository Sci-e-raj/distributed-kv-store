#include "snapshot_manager.h"
#include <fstream>

void SnapshotManager::save(const Snapshot& snap) {
    std::ofstream out(path_, std::ios::binary);
    out << snap.last_index << "\n";
    out << snap.data.size() << "\n";

    for (auto& [k, v] : snap.data) {
        out << k << " " << v << "\n";
    }
}

bool SnapshotManager::load(Snapshot& snap) {
    std::ifstream in(path_, std::ios::binary);
    if (!in.is_open()) return false;

    size_t size;
    in >> snap.last_index;
    in >> size;

    for (size_t i = 0; i < size; i++) {
        std::string k, v;
        in >> k >> v;
        snap.data[k] = v;
    }

    return true;
}
