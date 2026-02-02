#pragma once
#include <unordered_map>
#include <string>

struct Snapshot {
    int last_index;
    std::unordered_map<std::string, std::string> data;
};