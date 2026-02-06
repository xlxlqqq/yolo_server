#include "Dispatcher.h"
#include <string>

namespace server {

// 从请求字符串中提取 cmd 字段
static std::string getCmd(const std::string& req) {
    auto pos = req.find("\"cmd\"");
    if (pos == std::string::npos) return "";

    pos = req.find(":", pos);
    pos = req.find("\"", pos);
    auto end = req.find("\"", pos + 1);

    if (pos == std::string::npos || end == std::string::npos)
        return "";

    return req.substr(pos + 1, end - pos - 1);
}

// 简单的 Dispatcher 实现
std::string Dispatcher::dispatch(const std::string& request) {
    std::string cmd = getCmd(request);

    if (cmd == "ping") {
        return R"({"status":"ok","data":"pong"})";
    }

    if (cmd == "echo") {
        return request; // 先简单 echo 原请求
    }

    return R"({"status":"error","msg":"unknown command"})";
}


};  // namespace server