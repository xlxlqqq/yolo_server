#pragma once

#include <rocksdb/db.h>
#include <memory>

namespace storage {

class RocksDBStorage {
public:
    
    static RocksDBStorage& instance();

    bool open(const std::string& path);

    bool put(const std::string& key,
             const std::string& value);

    std::optional<std::string>
    get(const std::string& key);

private:
    RocksDBStorage();
    ~RocksDBStorage();

    RocksDBStorage(const RocksDBStorage&) = delete;
    RocksDBStorage(RocksDBStorage&&) = delete;
    RocksDBStorage& operator=(const RocksDBStorage&) = delete;
    RocksDBStorage& operator=(RocksDBStorage&&) = delete;

    std::unique_ptr<rocksdb::DB> m_db;
};

} // namespace storage
