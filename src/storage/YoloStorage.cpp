#include "storage/YoloStorage.h"
#include "common/logger/Logger.h"

namespace storage {

YoloStorage& YoloStorage::instance() {
    static YoloStorage inst;
    return inst;
}

// TODO: 使用存储引擎
bool YoloStorage::store(const YoloFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames[frame.image_id] = frame;

    LOG_INFO("stored YOLO frame: id={}, boxes={}",
             frame.image_id, frame.boxes.size());
    return true;
}

// TOOO: 使用存储引擎
std::optional<YoloFrame> YoloStorage::get(const std::string& image_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_frames.find(image_id);
    if (it == m_frames.end()) {
        return std::nullopt;
    }
    return it->second;
}


}  // namespace storage