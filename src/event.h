#pragma once
#include <string>
#include <functional>

enum class EventType {
    CLIENT_PUT,
    CLIENT_GET,
    REPL_ACK,
    HEARTBEAT_TICK,
    ELECTION_TIMEOUT,
    APPEND_ENTRIES_REQUEST,
    APPEND_ENTRIES_RESPONSE,
    VOTE_REQUEST,
    VOTE_RESPONSE
};

struct Event {
    EventType type;

    // For PUT/GET operations
    int index = -1;
    std::string key;
    std::string value;
    
    // For client response callback
    std::function<void(bool, const std::string&)> client_callback;

    // For ACK/Response events
    int ack_index = -1;
    bool success = false;
    
    // For Raft consensus
    int term = 0;
    int candidate_id = -1;
    int prev_log_index = -1;
    int prev_log_term = -1;
    int leader_commit = -1;
    
    // For vote responses
    bool vote_granted = false;
    int voter_id = -1;
};
