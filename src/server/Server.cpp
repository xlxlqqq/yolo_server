#include "server/Server.h"
#include "common/logger/Logger.h"
#include "common/config/Config.h"
#include "server/Connection.h"

#include <thread>
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

namespace server {
    
// TODO: 从配置文件加载集群节点信息
Server::Server()
    : m_running(false),
      m_router({
          {"node1", "127.0.0.1", 8080},
          {"node2", "127.0.0.1", 8081}
      }) {
    m_port = config::Config::instance().getInt("server.port", 9000);

    m_self.id = "node-1";
    m_self.host = "127.0.0.1";
    m_self.port = m_port;

}

Server::~Server() {
    if (m_running) {
        stop();
    }
}

bool Server::init() {
    LOG_INFO("server init");
    return true;
}

bool Server::start() {
    if (!setupSocket()) {
        return false;
    }

    m_running = true;
    LOG_INFO("server listening on port {}", m_port);

    acceptLoop();
    return true;
}

void Server::stop() {
    if (!m_running) return;

    LOG_INFO("server stopping...");
    m_running = false;

    if (m_listen_fd >= 0) {
        close(m_listen_fd);
        m_listen_fd = -1;
    }
}

void Server::wait() {
    LOG_INFO("server running, waiting...");

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool Server::isRunning() const {
    return m_running;
}

// 准备一个可以对外接客的监听 socket
bool Server::setupSocket() {
    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);  // 向操作系统申请一个AF_INET：IPv4 SOCK_STREAM：TCP的socket，返回fd
    if (m_listen_fd < 0) {
        LOG_ERROR("socket create failed");
        return false;
    }
    
    // 允许端口快速复用
    int opt = 1;  
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // sockaddr_in 是 IPv4 专用地址结构
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    addr.sin_port = htons(m_port);  // 字节序

    // 绑定端口
    if (bind(m_listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("bind failed");
        return false;
    }

    if (listen(m_listen_fd, 128) < 0) {
        LOG_ERROR("listen failed");
        return false;
    }

    return true;
}

// 心跳
void Server::acceptLoop() {
    while (m_running) {
        int client_fd = accept(m_listen_fd, nullptr, nullptr);
        if (client_fd < 0) continue;

        std::thread([this, client_fd]() {
            server::Connection conn(client_fd, m_router, m_self);
            conn.start();
        }).detach();
    }
}



};  // namespace server