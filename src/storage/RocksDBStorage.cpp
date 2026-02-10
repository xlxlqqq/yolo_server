#include "RocksDBStorage.h"
#include "common/logger/Logger.h"

#include <rocksdb/options.h>
#include <rocksdb/status.h>

namespace storage {

RocksDBStorage& RocksDBStorage::instance() {
    static RocksDBStorage inst;
    return inst;
}

RocksDBStorage::RocksDBStorage() {}

RocksDBStorage::~RocksDBStorage() {
    // unique_ptr 会自动关闭 DB
}

bool RocksDBStorage::open(const std::string& db_path) {
    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* db = nullptr;
    auto status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
        LOG_ERROR("open rocksdb failed: {}", status.ToString());
        throw std::runtime_error("failed to open rocksdb");
    }

    m_db.reset(db);
    return true;
}


bool RocksDBStorage::put(const std::string& key,
                         const std::string& value) {
    rocksdb::Status s =
        m_db->Put(rocksdb::WriteOptions(), key, value);

    if (!s.ok()) {
        LOG_ERROR("rocksdb put failed: {}", s.ToString());
        return false;
    }
    return true;
}

std::optional<std::string>
RocksDBStorage::get(const std::string& key) {
    std::string value;
    rocksdb::Status s =
        m_db->Get(rocksdb::ReadOptions(), key, &value);

    if (s.IsNotFound()) {
        return std::nullopt;
    }
    if (!s.ok()) {
        LOG_ERROR("rocksdb get failed: {}", s.ToString());
        return std::nullopt;
    }

    return value;
}

} // namespace storage
