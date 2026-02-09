#pragma once

#include <netinet/in.h>
#include <string>

#include "cluster/ShardRouter.h"
#include "cluster/NodeInfo.h"
namespace server {

class Connection {
public:
    explicit Connection(int fd,
               cluster::ShardRouter& router,
               const cluster::NodeInfo& self);

    ~Connection();

    void start();  // 链接主逻辑


private:
    void handle();

    void handlePing();
    void handleUnknown(const std::string& msg);
    void handleStore(const std::string& msg);
    void handleGet(const std::string& msg) const ;

    int forwardToNode(const cluster::NodeInfo& node, const std::string& msg) const;

    static bool connectWithRetry(int fd, const sockaddr& addr, const std::string& host,
        int port, int maxRetries = 5, int retryDelayMs = 1000);

private:
    int m_fd;
    cluster::ShardRouter& m_router;
    const cluster::NodeInfo& m_self;

};

};  // namespace server