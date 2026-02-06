#pragma once

#include <string>

namespace server {

enum class CommandType {
    PING,
    STORE,
    GET,
    UNKNOWN

};  // enum class CommandType

CommandType parseCommand(const std::string& cmd);

};