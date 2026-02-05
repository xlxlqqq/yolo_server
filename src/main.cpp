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


int main() {
    // TODO: 异步日志增加性能

    config::Config::instance().loadFromFile("../config/server.conf");

    int port = config::Config::instance().getInt("server.port", 8080);
    LOG_INFO("server will listen on port {}", port);
    
    logger::Logger::instance().init(
        "logs/server.log",
        logger::LogLevel::DEBUG
    );

    server::Server server;
    g_server = &server;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    if (!server.init()) {
        LOG_ERROR("server init failed");
        return 1;
    }

    server.start();
    server.wait();

    

    LOG_INFO("server exit");
    return 0;
}
