#include "server/Server.h"
#include "common/logger/Logger.h"
#include "common/config/Config.h"
#include "server/Connection.h"
#include "cluster/ShardRouter.h"

#include <thread>
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>


namespace server {
    
// TODO: 从配置文件加载集群节点信息
Server::Server()
    : m_running(false), m_router({}),
    m_thread_pool(std::thread::hardware_concurrency()) {

    if (m_thread_pool.size() == 0) {
        LOG_WARN("hardware_concurrency returned 0, fallback to 4 threads");
    }
    LOG_INFO("thread pool initialized with {} threads", m_thread_pool.size());

    m_port = config::Config::instance().getInt("server.port", 9000);

    m_self.id = config::Config::instance().getString("server.id");
    m_self.host = config::Config::instance().getString("server.host");
    m_self.port = m_port;

    size_t node_count = config::Config::instance().getInt("nodes.count", 0);
    std::vector<cluster::NodeInfo> nodes;
    for (size_t i = 0; i < node_count; ++i) {
        std::string prefix = "node" + std::to_string(i + 1);
        LOG_INFO("loading node config: {}", prefix);
        cluster::NodeInfo node;
        node.id = config::Config::instance().getString(prefix + ".id");
        node.host = config::Config::instance().getString(prefix + ".host");
        node.port = config::Config::instance().getInt(prefix + ".port");
        if (m_self.id == node.id) {
            LOG_INFO("skip self node config: {}", node.id);
            node.healthy = true;  // 自身肯定是健康的
        } else {
            node.healthy = cluster::checkNodeHealth(node);
        }

        if (node.healthy) {
            LOG_INFO("node {} is healthy", node.id);
        } else {
            LOG_WARN("node {} is unhealthy, will not be added to router", node.id);
            continue;  // 不健康的节点不加入路由
        }

        if(m_router.insertNode(node)) {
            LOG_INFO("{} added to router, now nodes count {}", node.id, m_router.getNodeCount());
        } else {
            LOG_WARN("{} already exists in router, now nodes count {}", node.id, m_router.getNodeCount());
        }
    }

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

    int flags = fcntl(m_listen_fd, F_GETFL, 0);
    fcntl(m_listen_fd, F_SETFL, flags | O_NONBLOCK);
    
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
        struct pollfd pfd{};
        pfd.fd = m_listen_fd;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, 1000);  // 等待1秒
        if (ret == 0) {
            continue;  // timeout，继续等待
        } 
        if (ret < 0) {
            if (errno == EINTR) {
                continue;  // 被信号打断，继续等待
            }
            LOG_ERROR("poll failed: {}", strerror(errno));
            break;
        }

        int client_fd = accept(m_listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            LOG_ERROR("accept error: {}", strerror(errno));
            continue;
        }

        m_thread_pool.submit([this, client_fd]() {
            server::Connection conn(client_fd, m_router, m_self);
            conn.start();
        });
    }
}



};  // namespace server