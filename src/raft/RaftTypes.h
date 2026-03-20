#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace raft {

// Raft 节点角色状态
enum class RaftState {
    Follower,
    Candidate,
    Leader
};

// 日志条目
struct LogEntry {
    uint64_t term;      // 该条目被创建时的任期号
    uint64_t index;     // 日志索引
    std::string key;    // 操作的 Key (针对 YOLO 数据通常是 image_id)
    std::string value;  // 操作的 Value (JSON 序列化后的包)
};

// RequestVote RPC 请求参数 (候选人发起)
struct RequestVoteArgs {
    uint64_t term;          // 候选人的任期号
    std::string candidateId;// 候选人 ID
    uint64_t lastLogIndex;  // 候选人最后一条日志的索引
    uint64_t lastLogTerm;   // 候选人最后一条日志的任期号
};

// RequestVote RPC 响应结果
struct RequestVoteReply {
    uint64_t term;          // 当前任期号，以便候选人更新自己的任期
    bool voteGranted;       // 候选人是否赢得选票
};

// AppendEntries RPC 请求参数 (leader发起：用于心跳和日志同步)
struct AppendEntriesArgs {
    uint64_t term;              // 领导者的任期号
    std::string leaderId;       // 领导者 ID，以便跟随者重定向客户端请求
    uint64_t prevLogIndex;      // 紧邻新日志条目之前的那个日志条目的索引
    uint64_t prevLogTerm;       // prevLogIndex 对应的任期号
    std::vector<LogEntry> entries; // 准备存储的日志条目（心跳时为空）
    uint64_t leaderCommit;      // 领导者的已知已提交的最高的日志条目的索引
};

// AppendEntries RPC 响应结果
struct AppendEntriesReply {
    uint64_t term;              // 当前任期号，用于领导者更新自己的任期
    bool success;               // 如果跟随者包含匹配 prevLogIndex 和 prevLogTerm 的日志条目，则为 true
};

} // namespace raft
