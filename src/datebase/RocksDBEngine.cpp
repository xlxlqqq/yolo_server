#include "storage/RocksDBEngine.h"
#include "common/logger/Logger.h"

using namespace rocksdb;

namespace storage {

RocksDBEngine::RocksDBEngine(const std::string& path) {
    Options options;
    options.create_if_missing = true;

    Status s = DB::Open(options, path, &m_db);
    if (!s.ok()) {
        LOG_FATAL("failed to open rocksdb: {}", s.ToString());
        throw std::runtime_error(s.ToString());
    }

    LOG_INFO("rocksdb opened at {}", path);
}

RocksDBEngine::~RocksDBEngine() {
    m_db.reset();
}

bool RocksDBEngine::put(
    const std::string& key,
    const std::string& value) {

    WriteOptions wo;
    wo.sync = false;   // 性能优先，后面可调

    Status s = m_db->Put(wo, key, value);
    if (!s.ok()) {
        LOG_ERROR("rocksdb put failed: {}", s.ToString());
        return false;
    }
    return true;
}

std::optional<std::string>
RocksDBEngine::get(const std::string& key) {

    ReadOptions ro;
    std::string value;

    Status s = m_db->Get(ro, key, &value);
    if (s.IsNotFound()) {
        return std::nullopt;
    }
    if (!s.ok()) {
        LOG_ERROR("rocksdb get failed: {}", s.ToString());
        return std::nullopt;
    }
    return value;
}

}
