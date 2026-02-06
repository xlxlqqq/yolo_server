#pragma once

#include <vector>
#include <string>

namespace storage {

struct YoloBox {
    int class_id;
    float x;
    float y;
    float w;
    float h;
    float confidence;
};  // class YoloBox

struct YoloFrame {
    std::string image_id;
    std::string image_hash;

    std::vector<YoloBox> boxes;
    int width;
    int height;
};  // class YoloFrame

};  // namespace storage