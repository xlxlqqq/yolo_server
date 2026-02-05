#include "server/Connection.h"
#include "common/logger/Logger.h"

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
    sockaddr_in peer{};
    socklen_t len = sizeof(peer);
    getpeername(m_fd, (sockaddr*)&peer, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    int port = ntohs(peer.sin_port);

    LOG_INFO("client connected: {}:{}", ip, port);

    char buffer[1024];
    while (true) {
        ssize_t n = recv(m_fd, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            break;
        }
        send(m_fd, buffer, n, 0);  // echo
    }

    LOG_INFO("client disconnected: {}:{}", ip, port);
}

};