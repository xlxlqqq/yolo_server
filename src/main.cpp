#include "common/logger/Logger.h"
#include "common/logger/Logger.h"
#include "server/Server.h"
#include "common/config/Config.h"

#include <csignal>
#include <spdlog/spdlog.h>

using namespace config;

// 让中断函数也能访问到 server 对象
static server::Server* g_server = nullptr;

// 中断服务函数
// 固定格式 void Func(int) 
void handleSignal(int sig) {
    LOG_WARN("received signal {}, shutting down", sig);
    if (g_server) {
        g_server->stop();
    }
}

// 命令行参数解析
std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path = "./config/server.conf"; // 默认

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[i + 1];
            break;
        }
    }

    return config_path;
}


int main(int argc, char* argv[]) {
    // TODO: 异步日志增加性能

    auto config_path = parseConfigPath(argc, argv);
    LOG_INFO("using config file: {}", config_path);

    if (!config::Config::instance().loadFromFile(config_path)) {
        LOG_ERROR("failed to load config, exit");
        return 1;
    }

    server::Server server;
    server.start();
    server.wait();
    return 0;
}
