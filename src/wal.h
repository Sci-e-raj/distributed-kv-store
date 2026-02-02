#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include "store.h"

struct LogEntry {
    int index;
    int term;
    std::string key;
    std::string value;
    std::string operation; // "PUT", "DELETE", etc.
    
    LogEntry() : index(-1), term(-1) {}
    LogEntry(int idx, int t, const std::string& k, const std::string& v, const std::string& op = "PUT")
        : index(idx), term(t), key(k), value(v), operation(op) {}
};

class WriteAheadLog {
public:
    explicit WriteAheadLog(const std::string& filename);
    
    // Append a log entry
    void appendEntry(const LogEntry& entry);
    
    // Get entry at index (1-indexed)
    bool getEntry(int index, LogEntry& entry) const;
    
    // Get the last log entry
    bool getLastEntry(LogEntry& entry) const;
    
    // Get last log index and term
    void getLastLogInfo(int& last_index, int& last_term) const;
    
    // Truncate log from index onwards (for conflict resolution)
    void truncateFrom(int index);
    
    // Replay all entries to rebuild state
    void replay(KVStore& store);
    
    // Get all entries from start_index onwards
    std::vector<LogEntry> getEntriesFrom(int start_index) const;
    
    // Persist metadata (current_term, voted_for)
    void saveMetadata(int current_term, int voted_for);
    
    // Load metadata
    void loadMetadata(int& current_term, int& voted_for);
    
    int size() const;

private:
    std::string filename_;
    std::string metadata_filename_;
    mutable std::vector<LogEntry> log_cache_;  // In-memory cache
    mutable std::mutex mutex_;
    
    void rebuildCache();
    void persistCache();
};