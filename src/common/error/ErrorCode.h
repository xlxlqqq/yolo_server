#pragma once

#include <cstdint>

enum ErrorCode {
    OK = 0,

    // ===== 通用错误 (1xxx) =====
    INVALID_REQUEST = 1000,
    INTERNAL_ERROR  = 1001,
    TIMEOUT         = 1002,

    // ===== 网络 / 转发 (2xxx) =====
    CONNECT_FAILED  = 2000,
    CONNECT_TIMEOUT = 2001,
    SEND_FAILED     = 2002,
    RECV_FAILED     = 2003,
    NODE_UNAVAILABLE = 2004,
    FD_FIALED       = 2005,

    // ===== 存储相关 (3xxx) =====
    NOT_FOUND       = 3000,
    ALREADY_EXISTS  = 3001,

    // ===== 集群 / shard (4xxx) =====
    NOT_LEADER      = 4000,
    SHARD_MISMATCH  = 4001,
};
