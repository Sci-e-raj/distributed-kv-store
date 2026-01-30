#include "wal.h"
#include <sstream>

WriteAheadLog::WriteAheadLog(const std::string& filename)
    : filename_(filename) {}

void WriteAheadLog::appendPut(const std::string& key, const std::string& value) {
    std::ofstream out(filename_, std::ios::app);
    out << "PUT " << key << " " << value << "\n";
    out.flush();
    // fsync(fileno(out.rdbuf()->fd()));
}

void WriteAheadLog::replay(KVStore& store) {
    std::ifstream in(filename_);
    std::string line;

    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string cmd, key, value;
        iss >> cmd >> key >> value;
        if (cmd == "PUT") {
            store.put(key, value);
        }
    }
}
