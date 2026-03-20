#pragma once

#include <netinet/in.h>
#include <string>
#include <memory>

#include "cluster/ShardRouter.h"
#include "cluster/NodeInfo.h"
#include "raft/RaftNode.h"

namespace server {

class Connection {
public:
    explicit Connection(int fd,
               cluster::ShardRouter& router,
               const cluster::NodeInfo& self,
               std::shared_ptr<raft::RaftNode> raft_node = nullptr);

    ~Connection();

    void start();  // 链接主逻辑


private:
    void handle();

    void handlePing();
    void handleUnknown(const std::string& msg);
    void handleStore(const std::string& msg);
    void handleGet(const std::string& msg) const ;
    void handleRaftRequestVote(const std::string& msg);
    void handleRaftAppendEntries(const std::string& msg);

    int forwardToNode(const cluster::NodeInfo& node, const std::string& msg) const;

    static bool connectWithRetry(int fd, const sockaddr& addr, const std::string& host,
        int port, int maxRetries = 5, int retryDelayMs = 1000);

private:
    int m_fd;
    cluster::ShardRouter& m_router;
    const cluster::NodeInfo& m_self;
    std::shared_ptr<raft::RaftNode> m_raft_node;

};

};  // namespace server