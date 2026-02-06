#include "server/Protocol.h"
#include "common/logger/Logger.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

namespace server {
    
static bool recvAll(int fd, void* buf, size_t len) {
    char* p = static_cast<char*>(buf);
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(fd, p + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool Protocol::recvMessage(int fd, std::vector<char>& out) {
    uint32_t len = 0;
    
    // 先收一个长度
    ssize_t n = recv(fd, &len, sizeof(len), MSG_WAITALL);
    if (n <= 0) {
        return false;
    }

    len = ntohl(len);
    if (len == 0 || len > MAX_MESSAGE_SIZE) {
        LOG_WARN("invalid message size: {}", len);
        return false;  // 无效的消息长度
    }

    out.resize(len);
    
    n = recv(fd, out.data(), len, MSG_WAITALL);  // 真正的消息体
    if (n <= 0) {
        return false;
    }

    return true;
}

bool Protocol::sendMessage(int fd, const std::vector<char>& data) {
    uint32_t len = data.size();
    uint32_t len_net = htonl(len);

    if (send(fd, &len_net, sizeof(len_net), 0) != sizeof(len_net))
        return false;

    if (send(fd, data.data(), data.size(), 0) != (ssize_t)data.size())
        return false;

    return true;
}

};  // namespace server