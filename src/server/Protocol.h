#pragma once

#include <cstdint>
#include <vector>

namespace server {

class Protocol {
public:
    static constexpr uint32_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB

    // 从 socket 读取一条完整消息
    static bool recvMessage(int fd, std::vector<char>& out);

    // 向 socket 发送一条完整消息
    static bool sendMessage(int fd, const std::vector<char>& data);

private:


};  // class Protocol

};  // namespace server