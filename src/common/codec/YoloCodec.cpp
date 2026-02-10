#include "common/codec/YoloCodec.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace storage {

std::string encode(const YoloFrame& f) {
    json j;
    j["image_id"] = f.image_id;
    j["width"] = f.width;
    j["height"] = f.height;
    j["image_hash"] = f.image_hash;

    j["boxes"] = json::array();
    for (const auto& b : f.boxes) {
        j["boxes"].push_back({
            {"class_id", b.class_id},
            {"x", b.x},
            {"y", b.y},
            {"w", b.w},
            {"h", b.h},
            {"confidence", b.confidence}
        });
    }
    return j.dump();
}

YoloFrame decode(const std::string& v) {
    json j = json::parse(v);
    YoloFrame f;
    f.image_id = j.at("image_id");
    f.width = j.at("width");
    f.height = j.at("height");
    f.image_hash = j.at("image_hash");

    for (auto& jb : j.at("boxes")) {
        YoloBox b;
        b.class_id = jb.at("class_id");
        b.x = jb.at("x");
        b.y = jb.at("y");
        b.w = jb.at("w");
        b.h = jb.at("h");
        b.confidence = jb.at("confidence");
        f.boxes.push_back(b);
    }
    return f;
}

}  // namespace storage
