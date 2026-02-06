#include "server/Command.h"

namespace server {

CommandType parseCommand(const std::string& cmd) {
    if (cmd == "PING") {
        return CommandType::PING;
    } else if (cmd == "STORE") {
        return CommandType::STORE;
    } else if (cmd == "GET") {
        return CommandType::GET;
    } else {
        return CommandType::UNKNOWN;
    }
}

};  // namespace server