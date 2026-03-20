#include "server/Command.h"

namespace server {

CommandType parseCommand(const std::string& cmd) {
    if (cmd == "PING") {
        return CommandType::PING;
    } else if (cmd == "STORE") {
        return CommandType::STORE;
    } else if (cmd == "GET") {
        return CommandType::GET;
    } else if (cmd == "RAFT_REQUEST_VOTE") {
        return CommandType::RAFT_REQUEST_VOTE;
    } else if (cmd == "RAFT_APPEND_ENTRIES") {
        return CommandType::RAFT_APPEND_ENTRIES;
    } else {
        return CommandType::UNKNOWN;
    }
}

};  // namespace server