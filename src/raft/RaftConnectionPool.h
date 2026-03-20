#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <netinet/in.h>

namespace raft {

class RaftConnectionPool {
public:
    RaftConnectionPool();
    ~RaftConnectionPool();

    // 获取一个连接
    int getConnection(const std::string& peerId, const std::string& host, int port);
    
    // 释放连接
    void releaseConnection(const std::string& peerId, int fd);
    
    // 关闭指定节点的所有连接
    void closePeerConnections(const std::string& peerId);
    
    // 启动连接池的后台线程
    void start();
    
    // 停止连接池
    void stop();

private:
    struct ConnectionInfo {
        int fd;
        std::string host;
        int port;
        std::chrono::steady_clock::time_point lastUsed;
        bool inUse;
    };

    // 创建新连接
    int createConnection(const std::string& host, int port);
    
    // 检查连接是否有效
    bool isConnectionValid(int fd);
    
    // 后台线程：清理过期连接
    void cleanupLoop();

private:
    // peer_id -> 连接队列
    std::unordered_map<std::string, std::queue<ConnectionInfo>> m_connectionPools;
    
    // peer_id -> 连接信息映射（用于跟踪所有连接）
    std::unordered_map<int, std::string> m_fdToPeerMap;
    
    std::mutex m_mutex;
    std::condition_variable m_cv;
    
    std::atomic<bool> m_running;
    std::thread m_cleanupThread;
    
    // 连接超时时间（秒）
    static constexpr int CONNECTION_TIMEOUT = 300;
    // 每个节点的最大连接数
    static constexpr size_t MAX_CONNECTIONS_PER_PEER = 5;
    // 清理线程的检查间隔（秒）
    static constexpr int CLEANUP_INTERVAL = 60;
};

} // namespace raft