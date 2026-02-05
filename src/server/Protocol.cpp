#include "server/Protocol.h"

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
    uint32_t netLen = 0;
    if (!recvAll(fd, &netLen, sizeof(netLen))) {
        return false;
    }

    uint32_t len = ntohl(netLen);
    if (len == 0 || len > 10 * 1024 * 1024) { // 防炸内存
        return false;
    }

    out.resize(len);
    return recvAll(fd, out.data(), len);
}

bool Protocol::sendMessage(int fd, const std::vector<char>& data) {
    uint32_t len = data.size();
    uint32_t netLen = htonl(len);

    if (send(fd, &netLen, sizeof(netLen), 0) != sizeof(netLen)) {
        return false;
    }

    if (len > 0) {
        if (send(fd, data.data(), len, 0) != (ssize_t)len) {
            return false;
        }
    }
    return true;
}

};  // namespace server