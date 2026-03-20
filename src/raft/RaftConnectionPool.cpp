#include "RaftConnectionPool.h"
#include "common/logger/Logger.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>

namespace raft {

RaftConnectionPool::RaftConnectionPool()
    : m_running(false) {
}

RaftConnectionPool::~RaftConnectionPool() {
    stop();
}

void RaftConnectionPool::start() {
    if (m_running) return;
    m_running = true;
    m_cleanupThread = std::thread(&RaftConnectionPool::cleanupLoop, this);
    LOG_INFO("RaftConnectionPool started");
}

void RaftConnectionPool::stop() {
    if (!m_running) return;
    m_running = false;
    m_cv.notify_all();
    
    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }
    
    // 关闭所有连接
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [peerId, queue] : m_connectionPools) {
        while (!queue.empty()) {
            auto& conn = queue.front();
            if (conn.fd >= 0) {
                close(conn.fd);
            }
            queue.pop();
        }
    }
    m_connectionPools.clear();
    m_fdToPeerMap.clear();
    
    LOG_INFO("RaftConnectionPool stopped");
}

int RaftConnectionPool::getConnection(const std::string& peerId, const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 查找是否有可用的连接
    auto it = m_connectionPools.find(peerId);
    if (it != m_connectionPools.end() && !it->second.empty()) {
        auto& conn = it->second.front();
        
        // 检查连接是否有效
        if (isConnectionValid(conn.fd)) {
            conn.inUse = true;
            conn.lastUsed = std::chrono::steady_clock::now();
            int fd = conn.fd;
            it->second.pop();
            LOG_DEBUG("Reusing connection {} for peer {}", fd, peerId);
            return fd;
        } else {
            // 连接无效，关闭并移除
            close(conn.fd);
            m_fdToPeerMap.erase(conn.fd);
            it->second.pop();
        }
    }
    
    // 没有可用连接，创建新连接
    int fd = createConnection(host, port);
    if (fd >= 0) {
        ConnectionInfo conn;
        conn.fd = fd;
        conn.host = host;
        conn.port = port;
        conn.lastUsed = std::chrono::steady_clock::now();
        conn.inUse = true;
        
        m_fdToPeerMap[fd] = peerId;
        LOG_DEBUG("Created new connection {} for peer {}", fd, peerId);
        return fd;
    }
    
    return -1;
}

void RaftConnectionPool::releaseConnection(const std::string& peerId, int fd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 检查连接是否属于该节点
    auto it = m_fdToPeerMap.find(fd);
    if (it == m_fdToPeerMap.end() || it->second != peerId) {
        LOG_WARN("Connection {} does not belong to peer {}, closing", fd, peerId);
        close(fd);
        return;
    }
    
    // 检查连接是否仍然有效
    if (!isConnectionValid(fd)) {
        LOG_DEBUG("Connection {} is invalid, closing", fd);
        m_fdToPeerMap.erase(fd);
        close(fd);
        return;
    }
    
    // 将连接放回池中
    auto& queue = m_connectionPools[peerId];
    if (queue.size() < MAX_CONNECTIONS_PER_PEER) {
        ConnectionInfo conn;
        conn.fd = fd;
        conn.host = ""; // 不需要存储 host 和 port，因为已经在 m_fdToPeerMap 中
        conn.port = 0;
        conn.lastUsed = std::chrono::steady_clock::now();
        conn.inUse = false;
        
        queue.push(conn);
        LOG_DEBUG("Released connection {} for peer {}", fd, peerId);
    } else {
        // 池已满，关闭连接
        LOG_DEBUG("Connection pool for peer {} is full, closing connection {}", peerId, fd);
        m_fdToPeerMap.erase(fd);
        close(fd);
    }
}

void RaftConnectionPool::closePeerConnections(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_connectionPools.find(peerId);
    if (it == m_connectionPools.end()) {
        return;
    }
    
    // 关闭所有该节点的连接
    while (!it->second.empty()) {
        auto& conn = it->second.front();
        if (conn.fd >= 0) {
            close(conn.fd);
            m_fdToPeerMap.erase(conn.fd);
        }
        it->second.pop();
    }
    
    m_connectionPools.erase(it);
    LOG_INFO("Closed all connections for peer {}", peerId);
}

int RaftConnectionPool::createConnection(const std::string& host, int port) {
    // 创建 socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("socket creation failed");
        return -1;
    }
    
    // 设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 连接到 peer
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("invalid host: {}", host);
        close(fd);
        return -1;
    }
    
    // 连接（带重试）
    bool connected = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!connected) {
        LOG_ERROR("connect to {}:{} failed", host, port);
        close(fd);
        return -1;
    }
    
    return fd;
}

bool RaftConnectionPool::isConnectionValid(int fd) {
    if (fd < 0) {
        return false;
    }
    
    // 使用 MSG_PEEK 检查连接是否仍然有效
    char buffer[1];
    ssize_t result = recv(fd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);
    
    if (result == 0) {
        // 连接已关闭
        return false;
    } else if (result < 0) {
        // 检查错误类型
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 连接仍然有效，只是没有数据可读
            return true;
        } else {
            // 连接错误
            return false;
        }
    }
    
    // 有数据可读，连接仍然有效
    return true;
}

void RaftConnectionPool::cleanupLoop() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        // 等待清理间隔或停止信号
        m_cv.wait_for(lock, std::chrono::seconds(CLEANUP_INTERVAL), [this]() {
            return !m_running;
        });
        
        if (!m_running) {
            break;
        }
        
        // 清理过期连接
        auto now = std::chrono::steady_clock::now();
        for (auto& [peerId, queue] : m_connectionPools) {
            std::queue<ConnectionInfo> newQueue;
            
            while (!queue.empty()) {
                auto& conn = queue.front();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - conn.lastUsed).count();
                
                if (elapsed > CONNECTION_TIMEOUT || !isConnectionValid(conn.fd)) {
                    // 连接过期或无效，关闭
                    LOG_DEBUG("Closing expired/invalid connection {} for peer {}", 
                              conn.fd, peerId);
                    close(conn.fd);
                    m_fdToPeerMap.erase(conn.fd);
                } else {
                    // 连接仍然有效，保留
                    newQueue.push(conn);
                }
                queue.pop();
            }
            
            queue = std::move(newQueue);
        }
        
        LOG_DEBUG("Connection pool cleanup completed");
    }
}

} // namespace raft