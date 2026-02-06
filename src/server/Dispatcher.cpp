#include "Dispatcher.h"

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

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

std::string Dispatcher::dispatch(const std::string& request) {
    try {
        json req = json::parse(request);

        if (!req.contains("cmd")) {
            return R"({"status":"error","msg":"missing cmd"})";
        }

        std::string cmd = req["cmd"];

        if (cmd == "ping") {
            return json{
                {"status", "ok"},
                {"data", "pong"}
            }.dump();
        }

        if (cmd == "echo") {
            return json{
                {"status", "ok"},
                {"data", req.value("data", "")}
            }.dump();
        }

        return json{
            {"status", "error"},
            {"msg", "unknown command"}
        }.dump();

    } catch (const std::exception& e) {
        return json{
            {"status", "error"},
            {"msg", std::string("invalid json: ") + e.what()}
        }.dump();
    }
}


};  // namespace server