cmake_minimum_required(VERSION 3.10)

project(storage_server 
        VERSION 0.1
        LANGUAGES CXX)

# =========================
# 基本编译设置
# =========================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   

set(RocksDB_DIR "/usr/include/rocksdb")

# Debug / Release 区分
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_subdirectory(third_party/spdlog)

find_path(ROCKSDB_INCLUDE_DIR
    NAMES rocksdb/db.h
)

# 找库
find_library(ROCKSDB_LIBRARY
    NAMES rocksdb
)


add_executable(storage_server
    src/main.cpp
    src/server/Server.cpp
    src/common/logger/Logger.cpp
    src/common/config/Config.cpp
    src/server/Connection.cpp
    src/server/Protocol.cpp
    src/server/Dispatcher.cpp
    src/server/Command.cpp
    src/storage/YoloStorage.cpp
    src/cluster/ShardRouter.cpp
    src/storage/RocksDBStorage.cpp
    src/common/codec/YoloCodec.cpp
    src/common/thread/ThreadPool.cpp
)

# =========================
# 源文件
# =========================
set(SERVER_SOURCES
    src/main.cpp
    src/server/Server.cpp
    src/common/logger/Logger.cpp
    src/common/config/Config.cpp
    src/server/Connection.cpp
    src/server/Protocol.cpp
    src/server/Dispatcher.cpp
    src/server/Command.cpp
    src/storage/YoloStorage.cpp
    src/storage/RocksDBStorage.cpp
    src/common/codec/YoloCodec.cpp
    src/common/thread/ThreadPool.cpp
)

# =========================
# 头文件搜索路径
# =========================
target_include_directories(storage_server
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/third_party/spdlog/include
        /usr/include/rocksdb
)

include_directories(${PROJECT_SOURCE_DIR}/third_party)


# =========================
# 链接库
# =========================
target_link_libraries(storage_server
    PRIVATE
        spdlog
)

target_include_directories(storage_server PRIVATE ${ROCKSDB_INCLUDE_DIR})
target_link_libraries(storage_server PRIVATE ${ROCKSDB_LIBRARY})

