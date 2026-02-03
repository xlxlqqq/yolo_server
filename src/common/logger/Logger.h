#pragma once 

#include <string>
#include <memory>
#include <utility>            // for std::forward

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>   // 


#define LOG_DEBUG(fmt, ...) \
    logger::Logger::instance().log( \
        logger::LogLevel::DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    logger::Logger::instance().log( \
        logger::LogLevel::INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    logger::Logger::instance().log( \
        logger::LogLevel::WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    logger::Logger::instance().log( \
        logger::LogLevel::ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

namespace logger {

enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR
};


class Logger {
public:
    static Logger& instance();

    // init for Logger
    void init(const std::string& log_file,
              LogLevel level = LogLevel::INFO,
              size_t max_file_size = 10 * 1024 * 1024,
              size_t max_files = 5);

    template<typename... Args>
    void log(LogLevel level,
             const char* file,
             int line,
             const char* func,
             fmt::format_string<Args...> fmt,
             Args&&... args) {

        spdlog::source_loc loc{file, line, func};

        switch (level) {
            case LogLevel::DEBUG:
                spdlog::log(loc, spdlog::level::debug, fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::INFO:
                spdlog::log(loc, spdlog::level::info, fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::WARN:
                spdlog::log(loc, spdlog::level::warn, fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::ERROR:
                spdlog::log(loc, spdlog::level::err, fmt, std::forward<Args>(args)...);
                break;
        }
    }

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    std::string logLevelToString(LogLevel level);

};  // class Logger

}  // namespace logger