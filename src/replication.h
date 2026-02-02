#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include "wal.h"

struct ReplicationState {
    int next_index;      // Next log index to send to this follower
    int match_index;     // Highest log index known to be replicated
    
    ReplicationState() : next_index(1), match_index(0) {}
};

class Replicator {
public:
    Replicator(const std::vector<std::string>& followers, int server_id);

    const std::vector<std::string>& followers() const;

    // Send heartbeats with AppendEntries (empty entries)
    void sendHeartbeats(int leader_term, int leader_commit, WriteAheadLog& wal);

    // Replicate new entries to followers
    // Returns true if majority acknowledged
    bool replicateEntries(int leader_term, int leader_commit, 
                         WriteAheadLog& wal, int start_index);
    
    // Update commit index based on match indices
    int calculateCommitIndex(int current_commit, int current_term, WriteAheadLog& wal);
    
    // Reset state for new leadership term
    void resetState(int last_log_index);

private:
    std::vector<std::string> followers_;
    int server_id_;
    
    // Track replication progress for each follower
    std::unordered_map<std::string, ReplicationState> follower_state_;
    std::mutex state_mutex_;
    
    // Send AppendEntries to a specific follower
    bool sendAppendEntries(const std::string& follower_addr,
                          int term,
                          int prev_log_index,
                          int prev_log_term,
                          const std::vector<LogEntry>& entries,
                          int leader_commit,
                          int& follower_next_index);
};
