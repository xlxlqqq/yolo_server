#pragma once

#include "RaftTypes.h"
#include "RaftConnectionPool.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <map>
#include <memory>

namespace raft {

class RaftNode {
public:
    RaftNode(const std::string& nodeId, const std::vector<std::string>& peers);
    ~RaftNode();

    // 启动/停止节点
    void start();
    void stop();

    // 暴露给外部网络层的 RPC 处理接口
    RequestVoteReply handleRequestVote(const RequestVoteArgs& args);
    AppendEntriesReply handleAppendEntries(const AppendEntriesArgs& args);

    // 客户端调用的接口：尝试提交一条日志
    // 如果当前不是 Leader，返回 false
    bool submitLog(const std::string& key, const std::string& value);

    // 设置节点地址映射
    void setPeerAddress(const std::string& peerId, const std::string& host, int port);

private:
    // 后台线程：定时器循环 (用于触发选举)
    void electionLoop();
    // 后台线程：Leader 发送心跳和日志循环
    void heartbeatLoop();
    
    // 触发选举
    void startElection();
    // 重置选举超时时间 (使用随机抖动)
    void resetElectionTimer();

    // 网络通信相关
    bool sendRequestVote(const std::string& peerId, const RequestVoteArgs& args, RequestVoteReply& reply);
    bool sendAppendEntries(const std::string& peerId, const AppendEntriesArgs& args, AppendEntriesReply& reply);
    
    // 状态机应用
    void applyCommittedLogs();

private:
    std::string m_nodeId;
    std::vector<std::string> m_peers;

    // 节点地址映射
    std::map<std::string, std::pair<std::string, int>> m_peerAddresses;
    
    // 连接池
    std::shared_ptr<RaftConnectionPool> m_connectionPool;

    std::mutex m_mutex;
    std::atomic<bool> m_running;

    // --- Raft 核心状态（所有服务器上的持久性状态） ---
    uint64_t m_currentTerm;
    std::string m_votedFor; // 当前任期内把票投给了谁
    std::vector<LogEntry> m_logs;

    // --- 所有服务器上的易失性状态 ---
    uint64_t m_commitIndex;
    uint64_t m_lastApplied;

    // --- Leader 上的易失性状态 (每次选举后重新初始化) ---
    // peer_id -> nextIndex / matchIndex
    std::unordered_map<std::string, uint64_t> m_nextIndex;
    std::unordered_map<std::string, uint64_t> m_matchIndex;

    // 当前节点的角色
    RaftState m_state;

    // 定时器相关
    std::thread m_electionThread;
    std::thread m_heartbeatThread;
    std::condition_variable m_electionCv;
    std::chrono::steady_clock::time_point m_lastHeartbeatTime;
    int m_electionTimeoutMs;
};

} // namespace raft
