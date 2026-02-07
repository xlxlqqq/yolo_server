#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace config {

class Config {
public:
    static Config& instance();

    void set(const std::string& key, const std::string& value);

    // 读取配置（带默认值）
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    bool loadConfig() const;

public:
    // 从配置文件加载配置
    bool loadFromFile(const std::string& path);

private:
    Config() = default;
    ~Config() = default;

    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;

    std::unordered_map<std::string, std::string> data_;
    mutable std::mutex mutex_;

};  // class Config

};  // namespace config