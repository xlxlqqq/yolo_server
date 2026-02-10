#include "cluster/ShardRouter.h"
#include "common/logger/Logger.h"

#include <functional>
#include <limits>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace cluster {

ShardRouter::ShardRouter(std::vector<NodeInfo> nodes) : m_nodes(std::move(nodes)) {}

const NodeInfo& ShardRouter::pickNode(const std::string& key) const {
    size_t best_score = 0;
    const NodeInfo* best_node = nullptr;

    LOG_INFO("picking node for key: {}", key);
    LOG_INFO("available nodes count : {}", m_nodes.size());

    for (const auto& node : m_nodes) {
        if (!isNodeHealthy(node)) {
            LOG_WARN("node {} is unhealthy, skipping", node.id);
            continue;
        }

        std::string s = key + "#" + node.id;
        size_t score = std::hash<std::string>{}(s);

        if (!best_node || score > best_score) {
            best_score = score;
            best_node = &node;
        }
    }

    if (!best_node) {
        LOG_ERROR("no healthy nodes available to route key: {}", key);
        // return nullptr;
    }

    return *best_node;
}

bool ShardRouter::insertNode(const NodeInfo& node) {
    for (const auto& n : m_nodes) {
        if (n.id == node.id) {
            return false;  // 已存在
        }
    }

    m_nodes.push_back(node);
    return true;
}

bool ShardRouter::isNodeHealthy(const NodeInfo& node) const {
    return node.healthy;
}

bool checkNodeHealth(const NodeInfo& node) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        LOG_ERROR("socket create failed for health check");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(node.port);

    if (inet_pton(AF_INET, node.host.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid node address: {}", node.host);
        close(sock);
        return false;
    }

    // 设置超时时间
    struct timeval timeout;
    timeout.tv_sec = 1;  // 1 秒超时
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // 尝试连接节点
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_WARN("Failed to connect to node {}:{}", node.id, node.port);
        close(sock);
        return false;
    }

    // 3. 发送心跳消息
    std::string ping = "PING";
    if (send(sock, ping.c_str(), ping.size(), 0) < 0) {
        LOG_WARN("Failed to send heartbeat to node {}", node.id);
        close(sock);
        return false;
    }

    // 4. 接收心跳响应
    char buffer[16] = {0};
    if (recv(sock, buffer, sizeof(buffer), 0) <= 0) {
        LOG_WARN("No response from node {}", node.id);
        close(sock);
        return false;
    }

    // 检查响应是否为预期值
    std::string response(buffer);
    if (response != "PONG") {
        LOG_WARN("Unexpected heartbeat response from node {}: {}", node.id, response);
        close(sock);
        return false;
    }

    // 5. 关闭连接并返回健康状态
    close(sock);
    LOG_INFO("Node {} is healthy", node.id);
    return true;
}

}  // namespace cluster