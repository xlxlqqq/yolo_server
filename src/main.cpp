#include "common/logger/Logger.h"
#include <spdlog/spdlog.h>

int main() {
    logger::Logger::instance().init(
        "logs/server.log",
        logger::LogLevel::DEBUG
    );

    LOG_INFO("storage server started");
    LOG_DEBUG("debug message test");

    return 0;
}
