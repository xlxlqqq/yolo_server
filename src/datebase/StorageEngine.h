#pragma once

#include <string>
#include <optional>

namespace storage {

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual bool put(
        const std::string& key,
        const std::string& value
    ) = 0;

    virtual std::optional<std::string> get(
        const std::string& key
    ) = 0;
};

}
