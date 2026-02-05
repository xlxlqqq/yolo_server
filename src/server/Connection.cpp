#include "server/Connection.h"
#include "common/logger/Logger.h"
#include "server/Protocol.h"

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace server {

Connection::Connection(int fd) : m_fd(fd) {}

Connection::~Connection() {
    if (m_fd >= 0) {
        close(m_fd);
    }
}

void Connection::start() {
    handle();
}

void Connection::handle() {
    LOG_INFO("client connected");

    while (true) {
        std::vector<char> msg;
        if (!server::Protocol::recvMessage(m_fd, msg)) {
            break;
        }

        // 👉 这里以后接 YOLO / JSON / 命令
        LOG_INFO("received message size={}", msg.size());

        // echo 回去（完整消息）
        server::Protocol::sendMessage(m_fd, msg);
    }

    LOG_INFO("client disconnected");
}

};