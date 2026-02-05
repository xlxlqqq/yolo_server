#include "common/config/Config.h"
#include "common/logger/Logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace config{

// 去除字符串首尾空白字符
static inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

Config& Config::instance() {
    static Config instance;
    return instance;
}

void Config::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key] = value;
}

std::string Config::getString(const std::string& key,
                              const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return defaultValue;
}

int Config::getInt(const std::string& key, int defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

bool Config::getBool(const std::string& key, bool defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        std::string v = it->second;
        std::transform(v.begin(), v.end(), v.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (v == "1" || v == "true" || v == "yes" || v == "on") {
            return true;
        }
        if (v == "0" || v == "false" || v == "no" || v == "off") {
            return false;
        }
    }
    return defaultValue;
}

bool Config::loadFromFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        LOG_ERROR("failed to open config file: {}", path);
        return false;
    }

    std::string line;
    int lineno = 0;

    while (std::getline(ifs, line)) {
        ++lineno;
        line = trim(line);

        // 空行 or 注释
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) {
            LOG_WARN("invalid config line {}: {}", lineno, line);
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (key.empty()) {
            LOG_WARN("empty key at line {}", lineno);
            continue;
        }

        set(key, value);
    }

    LOG_INFO("config loaded from {}", path);
    return true;
}

};
