#pragma once

#include <atomic>
#include <memory>

#include "cluster/NodeInfo.h"
#include "cluster/ShardRouter.h"
#include "common/thread/ThreadPool.h"
#include "raft/RaftNode.h" // 引入 Raft 节点

namespace server {

class Server {
public:
    Server();
    ~Server();

    bool init();
    bool start();
    void stop();
    void wait();

    bool isRunning() const;

    cluster::ShardRouter& router() { return m_router; }
    const cluster::NodeInfo& self() const { return m_self; }
    std::shared_ptr<raft::RaftNode> getRaftNode() const { return m_raft_node; }

private:
    std::atomic<bool> m_running;

    bool setupSocket();
    void acceptLoop();

    int m_listen_fd{-1};
    int m_epoll_fd{-1};  // epoll 文件描述符
    int m_port{8080};

    cluster::NodeInfo m_self;
    cluster::ShardRouter m_router;

    common::ThreadPool m_thread_pool;
    
    std::shared_ptr<raft::RaftNode> m_raft_node;

};

};  // namespace server