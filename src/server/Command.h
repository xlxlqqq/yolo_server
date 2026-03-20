#pragma once

#include <string>

namespace server {

enum class CommandType {
    PING,
    STORE,
    GET,
    RAFT_REQUEST_VOTE,    // Raft 选主请求
    RAFT_APPEND_ENTRIES,  // Raft 心跳/日志追加
    UNKNOWN

};  // enum class CommandType

CommandType parseCommand(const std::string& cmd);

};