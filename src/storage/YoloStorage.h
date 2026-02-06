#pragma once

#include "YoloTypes.h"

#include <unordered_map>
#include <mutex>
#include <optional>

namespace storage {

struct StoreRequest {
    std::string image_id;
    std::string image_hash;
};

class YoloStorage {
public:
    static YoloStorage& instance();

    bool store(const YoloFrame& frame);
    std::optional<YoloFrame> get(const std::string& image_id) const;

private:
    YoloStorage() = default;
    ~YoloStorage() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, YoloFrame> m_frames;


};  // class YoloStore

};  // namespace storage