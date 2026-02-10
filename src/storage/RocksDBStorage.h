#pragma once

#include "storage/YoloStorage.h"

#include <rocksdb/db.h>
#include <memory>

namespace storage {

class RocksDBStorage : public YoloStorage {
public:
    explicit RocksDBStorage(const std::string& db_path);
    ~RocksDBStorage();

    bool put(const std::string& key,
             const std::string& value) override;

    std::optional<std::string>
    get(const std::string& key) override;

private:
    std::unique_ptr<rocksdb::DB> m_db;
};

} // namespace storage
