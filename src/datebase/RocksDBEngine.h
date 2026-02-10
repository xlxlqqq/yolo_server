#pragma once
#include "storage/StorageEngine.h"
#include <rocksdb/db.h>
#include <memory>

namespace storage {

class RocksDBEngine : public StorageEngine {
public:
    explicit RocksDBEngine(const std::string& path);
    ~RocksDBEngine();

    bool put(const std::string& key,
             const std::string& value) override;

    std::optional<std::string> get(
        const std::string& key) override;

private:
    std::unique_ptr<rocksdb::DB> m_db;
};

}
