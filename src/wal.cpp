#include "wal.h"
#include <sstream>
#include <iostream>
#include <algorithm>

WriteAheadLog::WriteAheadLog(const std::string& filename)
    : filename_(filename), metadata_filename_(filename + ".meta") {
    rebuildCache();
}

void WriteAheadLog::appendEntry(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Append to file
    std::ofstream out(filename_, std::ios::app);
    if (!out) {
        std::cerr << "[ERROR] Failed to open WAL for writing: " << filename_ << std::endl;
        return;
    }
    
    out << entry.index << " " 
        << entry.term << " " 
        << entry.operation << " " 
        << entry.key << " " 
        << entry.value << "\n";
    out.flush();
    
    // Add to cache
    log_cache_.push_back(entry);
}

bool WriteAheadLog::getEntry(int index, LogEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Log is 1-indexed, cache is 0-indexed
    if (index < 1 || index > static_cast<int>(log_cache_.size())) {
        return false;
    }
    
    entry = log_cache_[index - 1];
    return true;
}

bool WriteAheadLog::getLastEntry(LogEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (log_cache_.empty()) {
        return false;
    }
    
    entry = log_cache_.back();
    return true;
}

void WriteAheadLog::getLastLogInfo(int& last_index, int& last_term) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (log_cache_.empty()) {
        last_index = 0;
        last_term = 0;
    } else {
        last_index = log_cache_.back().index;
        last_term = log_cache_.back().term;
    }
}

void WriteAheadLog::truncateFrom(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (index < 1 || index > static_cast<int>(log_cache_.size())) {
        return;
    }
    
    // Remove from cache (index is 1-indexed)
    log_cache_.erase(log_cache_.begin() + (index - 1), log_cache_.end());
    
    // Rewrite entire file
    persistCache();
}

void WriteAheadLog::replay(KVStore& store) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& entry : log_cache_) {
        if (entry.operation == "PUT") {
            store.put(entry.key, entry.value);
        } else if (entry.operation == "DELETE") {
            // Future: implement delete in store
            std::string dummy;
            store.get(entry.key, dummy); // placeholder
        }
    }
    
    std::cout << "[INFO] Replayed " << log_cache_.size() << " log entries\n";
}

std::vector<LogEntry> WriteAheadLog::getEntriesFrom(int start_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<LogEntry> result;
    
    if (start_index < 1 || start_index > static_cast<int>(log_cache_.size())) {
        return result;
    }
    
    for (size_t i = start_index - 1; i < log_cache_.size(); i++) {
        result.push_back(log_cache_[i]);
    }
    
    return result;
}

void WriteAheadLog::saveMetadata(int current_term, int voted_for) {
    std::ofstream out(metadata_filename_);
    if (!out) {
        std::cerr << "[ERROR] Failed to save metadata\n";
        return;
    }
    
    out << current_term << " " << voted_for << "\n";
    out.flush();
}

void WriteAheadLog::loadMetadata(int& current_term, int& voted_for) {
    std::ifstream in(metadata_filename_);
    if (!in) {
        // File doesn't exist, use defaults
        current_term = 0;
        voted_for = -1;
        return;
    }
    
    in >> current_term >> voted_for;
    std::cout << "[INFO] Loaded metadata: term=" << current_term 
              << ", voted_for=" << voted_for << std::endl;
}

int WriteAheadLog::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(log_cache_.size());
}

void WriteAheadLog::rebuildCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    log_cache_.clear();
    
    std::ifstream in(filename_);
    if (!in) {
        // File doesn't exist yet, that's okay
        return;
    }
    
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        LogEntry entry;
        iss >> entry.index >> entry.term >> entry.operation >> entry.key >> entry.value;
        
        log_cache_.push_back(entry);
    }
    
    std::cout << "[INFO] Loaded " << log_cache_.size() << " entries from WAL\n";
}

void WriteAheadLog::persistCache() {
    // Assumes mutex is already held
    std::ofstream out(filename_, std::ios::trunc);
    if (!out) {
        std::cerr << "[ERROR] Failed to persist WAL cache\n";
        return;
    }
    
    for (const auto& entry : log_cache_) {
        out << entry.index << " " 
            << entry.term << " " 
            << entry.operation << " " 
            << entry.key << " " 
            << entry.value << "\n";
    }
    
    out.flush();
}
