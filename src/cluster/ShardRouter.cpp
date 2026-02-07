#include "cluster/ShardRouter.h"

#include <functional>
#include <limits>

namespace cluster {

ShardRouter::ShardRouter(std::vector<NodeInfo> nodes) : m_nodes(std::move(nodes)) {}

const NodeInfo& ShardRouter::pickNode(const std::string& key) const {
    size_t best_score = 0;
    const NodeInfo* best_node = nullptr;

    for (const auto& node : m_nodes) {
        std::string s = key + "#" + node.id;
        size_t score = std::hash<std::string>{}(s);

        if (!best_node || score > best_score) {
            best_score = score;
            best_node = &node;
        }
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

}  // namespace cluster