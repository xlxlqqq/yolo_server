#include "server/Connection.h"
#include "common/logger/Logger.h"
#include "server/Protocol.h"
#include "server/Dispatcher.h"
#include "server/Command.h"

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <vector>

namespace server {

Connection::Connection(int fd) : m_fd(fd) {}

Connection::~Connection() {
    if (m_fd >= 0) {
        LOG_INFO("connection fd {} closed", m_fd);
        close(m_fd);
    }
}

void Connection::start() {
    handle();
}

static std::string getFirstToken(const std::string& s) {
    std::istringstream iss(s);
    std::string token;
    iss >> token;
    return token;
}

void Connection::handle() {
    LOG_INFO("connection started, fd={}", m_fd);

    std::vector<char> data;
    
    while (Protocol::recvMessage(m_fd, data)) {
        std::string msg(data.begin(), data.end());

        LOG_INFO("received message: {}", msg);

        std::string cmd_str = getFirstToken(msg);
        CommandType cmd_type = parseCommand(cmd_str);

        switch (cmd_type) {
            case CommandType::PING:
                handlePing();
                break;
            case CommandType::STORE:
                handleStore(msg);
                break;
            case CommandType::GET:
                handleGet(msg);
                break;
            default:
                handleUnknown(msg);
                break;
        }
    }

    LOG_INFO("connection closed, fd={}", m_fd);
}

void Connection::handleUnknown(const std::string& msg) {
    LOG_WARN("unknown command: {}", msg);

    std::string reply = "ERROR unknown command";
    Protocol::sendMessage(m_fd,
        std::vector<char>(reply.begin(), reply.end()));
}

void Connection::handlePing() {
    std::string reply = "PONG";
    Protocol::sendMessage(m_fd,
        std::vector<char>(reply.begin(), reply.end()));
}


void Connection::handleStore(const std::string& msg) {
    LOG_INFO("STORE command received: {}", msg);

    std::string reply = "STORE OK (not implemented)";
    Protocol::sendMessage(m_fd,
        std::vector<char>(reply.begin(), reply.end()));
}

void Connection::handleGet(const std::string& msg) {
    LOG_INFO("GET command received: {}", msg);

    std::string reply = "GET OK (not implemented)";
    Protocol::sendMessage(m_fd,
        std::vector<char>(reply.begin(), reply.end()));
}

};