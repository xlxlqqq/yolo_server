#include "server/Server.h"
#include "common/logger/Logger.h"

#include <thread>
#include <chrono>

namespace server {
Server::Server() : running_(false) {}

Server::~Server() {
    if (running_) {
        stop();
    }
}

bool Server::init() {
    LOG_INFO("server init");
    return true;
}

void Server::start() {
    LOG_INFO("server start");
    running_.store(true);
}

void Server::stop() {
    LOG_WARN("server stopping...");
    running_.store(false);
    LOG_INFO("server stopped");
}

void Server::wait() {
    LOG_INFO("server running, waiting...");

    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool Server::isRunning() const {
    return running_.load();
}



};  // namespace server