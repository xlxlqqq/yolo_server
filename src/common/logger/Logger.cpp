#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "common/logger/Logger.h"

namespace logger {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

// 日志系统初始化
void Logger::init(const std::string& log_file,
                  LogLevel level,
                  size_t max_file_size,
                  size_t max_files) {

    try {
        // 1️⃣ 控制台 sink（彩色）
        auto console_sink =
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);

        // 2️⃣ 文件 sink（轮转）
        auto file_sink =
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file, max_file_size, max_files);
        file_sink->set_level(spdlog::level::debug);

        // 3️⃣ 组合 logger
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>(
            "default", sinks.begin(), sinks.end());

        // 4️⃣ 日志级别
        spdlog::level::level_enum spd_level = spdlog::level::info;
        switch (level) {
            case LogLevel::DEBUG: spd_level = spdlog::level::debug; break;
            case LogLevel::INFO:  spd_level = spdlog::level::info;  break;
            case LogLevel::WARN:  spd_level = spdlog::level::warn;  break;
            case LogLevel::ERROR: spd_level = spdlog::level::err;   break;
        }

        logger->set_level(spd_level);
        logger->flush_on(spd_level);

        // 5️⃣ 日志格式（强烈建议）
        logger->set_pattern(
            "[%Y-%m-%d %H:%M:%S.%e] "
            "[%^%l%$] "
            "[%s:%# %!] "
            "%v"
        );

        spdlog::set_default_logger(logger);
        spdlog::info("Logger initialized, file={}", log_file);

    } catch (const spdlog::spdlog_ex& ex) {
        fprintf(stderr, "Logger init failed: %s\n", ex.what());
    }
}

}  // namespace logger


