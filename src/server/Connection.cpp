#include "server/Connection.h"
#include "common/logger/Logger.h"
#include "server/Protocol.h"
#include "server/Dispatcher.h"

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

    std::vector<char> msg;

    while (Protocol::recvMessage(m_fd, msg)) {
        std::string request(msg.begin(), msg.end());

        std::string response = Dispatcher::dispatch(request);

        std::vector<char> out(response.begin(), response.end());
        Protocol::sendMessage(m_fd, out);
    }

    close(m_fd);
}

};