#pragma once

#include <string>

namespace cluster {

struct NodeInfo {
    std::string id;   // node-1, node-2 ...
    std::string host; // 127.0.0.1
    int port;         // 8080
};

}
