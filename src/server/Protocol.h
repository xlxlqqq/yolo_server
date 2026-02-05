#pragma once

#include <cstdint>
#include <vector>

namespace server {

class Protocol {
public:
    // 从 socket 读取一条完整消息
    static bool recvMessage(int fd, std::vector<char>& out);

    // 向 socket 发送一条完整消息
    static bool sendMessage(int fd, const std::vector<char>& data);

private:


};  // class Protocol

};  // namespace server