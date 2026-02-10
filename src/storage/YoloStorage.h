#pragma once

#include "YoloTypes.h"

#include <string>
#include <mutex>
#include <optional>

namespace storage {

struct StoreRequest {
    std::string image_id;
    std::string image_hash;
};

class YoloStorage {
public:
    virtual ~YoloStorage() = default;

    virtual bool put(const std::string& key,
                     const std::string& value) = 0;

    virtual std::optional<std::string>
    get(const std::string& key) = 0;

};  // class YoloStore

};  // namespace storage