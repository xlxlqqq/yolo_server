#pragma once

#include <string>

namespace cluster {

struct NodeInfo {
    std::string id;   // node1, node2 ...
    std::string host; // 127.0.0.1
    int port;         // 8080
};

}
