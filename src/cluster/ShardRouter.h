#pragma once

#include <string>
#include <vector>

#include "cluster/NodeInfo.h"

namespace cluster {

class ShardRouter {
public:
    ShardRouter(std::vector<NodeInfo> nodes);

    const NodeInfo& pickNode(const std::string& key) const;
    bool insertNode(const NodeInfo& node);

    int getNodeCount() const { return m_nodes.size(); }

private:
    std::vector<NodeInfo> m_nodes;

    bool isNodeHealthy(const NodeInfo& node) const;

};  // class ShardRouter

bool checkNodeHealth(const NodeInfo& node);


}  // namespace cluster