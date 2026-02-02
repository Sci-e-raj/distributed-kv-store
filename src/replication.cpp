#include "replication.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include <algorithm>
#include <thread>

Replicator::Replicator(const std::vector<std::string>& followers, int server_id)
    : followers_(followers), server_id_(server_id) {
    
    for (const auto& follower : followers_) {
        follower_state_[follower] = ReplicationState();
    }
}

const std::vector<std::string>& Replicator::followers() const {
    return followers_;
}

void Replicator::resetState(int last_log_index) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    for (auto& pair : follower_state_) {
        pair.second.next_index = last_log_index + 1;
        pair.second.match_index = 0;
    }
}

void Replicator::sendHeartbeats(int leader_term, int leader_commit, WriteAheadLog& wal) {
    for (const auto& follower : followers_) {
        std::thread([this, follower, leader_term, leader_commit, &wal]() {
            int dummy_next_index = 1;
            
            int prev_log_index, prev_log_term;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                prev_log_index = follower_state_[follower].next_index - 1;
            }
            
            LogEntry prev_entry;
            if (prev_log_index > 0 && wal.getEntry(prev_log_index, prev_entry)) {
                prev_log_term = prev_entry.term;
            } else {
                prev_log_term = 0;
            }
            
            std::vector<LogEntry> empty_entries;
            sendAppendEntries(follower, leader_term, prev_log_index, prev_log_term,
                            empty_entries, leader_commit, dummy_next_index);
        }).detach();
    }
}

bool Replicator::replicateEntries(int leader_term, int leader_commit,
                                  WriteAheadLog& wal, int start_index) {
    int acks = 1; // Leader counts as one
    std::mutex ack_mutex;
    std::vector<std::thread> threads;
    
    for (const auto& follower : followers_) {
        threads.emplace_back([&, follower]() {
            int prev_log_index, prev_log_term;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                prev_log_index = follower_state_[follower].next_index - 1;
            }
            
            LogEntry prev_entry;
            if (prev_log_index > 0 && wal.getEntry(prev_log_index, prev_entry)) {
                prev_log_term = prev_entry.term;
            } else {
                prev_log_term = 0;
            }
            
            std::vector<LogEntry> entries = wal.getEntriesFrom(prev_log_index + 1);
            
            int new_next_index = prev_log_index + 1;
            bool success = sendAppendEntries(follower, leader_term, prev_log_index,
                                            prev_log_term, entries, leader_commit,
                                            new_next_index);
            
            if (success) {
                std::lock_guard<std::mutex> lock(ack_mutex);
                acks++;
                
                // Update follower state
                std::lock_guard<std::mutex> state_lock(state_mutex_);
                if (!entries.empty()) {
                    follower_state_[follower].match_index = entries.back().index;
                    follower_state_[follower].next_index = entries.back().index + 1;
                }
            } else {
                // Decrement next_index and retry (handled in next heartbeat/replication)
                std::lock_guard<std::mutex> state_lock(state_mutex_);
                if (follower_state_[follower].next_index > 1) {
                    follower_state_[follower].next_index--;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    
    int cluster_size = static_cast<int>(followers_.size()) + 1;
    return acks > (cluster_size / 2);
}

int Replicator::calculateCommitIndex(int current_commit, int current_term, WriteAheadLog& wal) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    int last_log_index, last_log_term;
    wal.getLastLogInfo(last_log_index, last_log_term);
    
    // Find the highest index that's replicated on a majority
    for (int N = last_log_index; N > current_commit; N--) {
        LogEntry entry;
        if (!wal.getEntry(N, entry) || entry.term != current_term) {
            continue; // Only commit entries from current term
        }
        
        int replicas = 1; // Leader has it
        for (const auto& pair : follower_state_) {
            if (pair.second.match_index >= N) {
                replicas++;
            }
        }
        
        int cluster_size = static_cast<int>(followers_.size()) + 1;
        if (replicas > (cluster_size / 2)) {
            return N;
        }
    }
    
    return current_commit;
}

bool Replicator::sendAppendEntries(const std::string& follower_addr,
                                   int term,
                                   int prev_log_index,
                                   int prev_log_term,
                                   const std::vector<LogEntry>& entries,
                                   int leader_commit,
                                   int& follower_next_index) {
    std::string ip = follower_addr.substr(0, follower_addr.find(':'));
    int port = std::stoi(follower_addr.substr(follower_addr.find(':') + 1));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "[ERROR] Failed to create socket for " << follower_addr << std::endl;
        return false;
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv.sin_addr);

    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) {
        close(sock);
        return false;
    }

    std::ostringstream oss;
    oss << "APPEND_ENTRIES "
        << term << " "
        << server_id_ << " "
        << prev_log_index << " "
        << prev_log_term << " "
        << leader_commit << " "
        << entries.size();
    
    for (const auto& entry : entries) {
        oss << " " << entry.index
            << " " << entry.term
            << " " << entry.operation
            << " " << entry.key
            << " " << entry.value;
    }
    oss << "\n";

    std::string msg = oss.str();
    ssize_t written = write(sock, msg.c_str(), msg.size());
    if (written < 0) {
        close(sock);
        return false;
    }

    char buffer[256];
    ssize_t n = read(sock, buffer, sizeof(buffer));
    close(sock);

    if (n > 0) {
        std::string resp(buffer, n);
        std::istringstream iss(resp);
        std::string result;
        int resp_term, resp_next_index;
        
        iss >> result >> resp_term >> resp_next_index;
        
        if (result == "SUCCESS") {
            follower_next_index = resp_next_index;
            return true;
        } else if (result == "FAIL") {
            follower_next_index = resp_next_index;
            return false;
        }
    }

    return false;
}
