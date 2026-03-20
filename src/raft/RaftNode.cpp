#include "RaftNode.h"
#include "common/logger/Logger.h"

#include <random>
#include <chrono>

namespace raft {

RaftNode::RaftNode(const std::string& nodeId, const std::vector<std::string>& peers)
    : m_nodeId(nodeId),
      m_peers(peers),
      m_running(false),
      m_currentTerm(0),
      m_votedFor(""),
      m_commitIndex(0),
      m_lastApplied(0),
      m_state(RaftState::Follower) {
    
    // 初始化时加一条 dummy log，让 index 从 1 开始比较方便处理
    m_logs.push_back({0, 0, "", ""});
    resetElectionTimer();
}

RaftNode::~RaftNode() {
    stop();
}

void RaftNode::start() {
    if (m_running) return;
    m_running = true;

    LOG_INFO("RaftNode {} starting...", m_nodeId);

    m_electionThread = std::thread(&RaftNode::electionLoop, this);
    m_heartbeatThread = std::thread(&RaftNode::heartbeatLoop, this);
}

void RaftNode::stop() {
    if (!m_running) return;
    m_running = false;
    m_electionCv.notify_all();

    if (m_electionThread.joinable()) {
        m_electionThread.join();
    }
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    LOG_INFO("RaftNode {} stopped.", m_nodeId);
}

void RaftNode::resetElectionTimer() {
    // Raft 论文建议选举超时时间在 150ms ~ 300ms 之间随机，以防活锁
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(150, 300);
    
    m_electionTimeoutMs = dist(rng);
    m_lastHeartbeatTime = std::chrono::steady_clock::now();
}

void RaftNode::electionLoop() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastHeartbeatTime).count();

        if (m_state != RaftState::Leader && elapsed >= m_electionTimeoutMs) {
            // 超时了，转变为 Candidate 并发起选举
            startElection();
        }

        // 等待一小段时间再检查（或者被条件变量唤醒）
        m_electionCv.wait_for(lock, std::chrono::milliseconds(10));
    }
}

void RaftNode::startElection() {
    m_state = RaftState::Candidate;
    m_currentTerm++;
    m_votedFor = m_nodeId; // 给自己投票
    resetElectionTimer();

    LOG_INFO("Node {} starting election for term {}", m_nodeId, m_currentTerm);

    // 准备 RequestVote 参数
    RequestVoteArgs args;
    args.term = m_currentTerm;
    args.candidateId = m_nodeId;
    args.lastLogIndex = m_logs.back().index;
    args.lastLogTerm = m_logs.back().term;

    // TODO: 在这里通过网络层(如 RPC/TCP) 将 args 发送给 m_peers
    // 收到响应后统计选票。如果选票 > 节点数/2，则：
    // m_state = RaftState::Leader;
    // 初始化 m_nextIndex 和 m_matchIndex
    // 立即发送一次空日志作为心跳 (AppendEntries)
}

void RaftNode::heartbeatLoop() {
    while (m_running) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_state == RaftState::Leader) {
                // TODO: 构造 AppendEntriesArgs 并发送给所有 Follower
                // LOG_DEBUG("Leader {} sending heartbeats", m_nodeId);
            }
        }
        // 心跳间隔通常小于选举超时下限，比如 50ms
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

RequestVoteReply RaftNode::handleRequestVote(const RequestVoteArgs& args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    RequestVoteReply reply;
    reply.term = m_currentTerm;
    reply.voteGranted = false;

    // 1. 如果请求任期小于当前任期，拒绝投票
    if (args.term < m_currentTerm) {
        return reply;
    }

    // 如果发现更大的任期，退回 Follower
    if (args.term > m_currentTerm) {
        m_currentTerm = args.term;
        m_state = RaftState::Follower;
        m_votedFor = "";
    }

    // 2. 检查是否已经投过票，以及日志是否至少和自己一样新
    uint64_t myLastLogTerm = m_logs.back().term;
    uint64_t myLastLogIndex = m_logs.back().index;

    bool logIsUpToDate = (args.lastLogTerm > myLastLogTerm) || 
                         (args.lastLogTerm == myLastLogTerm && args.lastLogIndex >= myLastLogIndex);

    if ((m_votedFor == "" || m_votedFor == args.candidateId) && logIsUpToDate) {
        m_votedFor = args.candidateId;
        reply.voteGranted = true;
        resetElectionTimer(); // 投票后重置选举定时器
        LOG_INFO("Node {} voted for {} in term {}", m_nodeId, args.candidateId, m_currentTerm);
    }

    reply.term = m_currentTerm;
    return reply;
}

AppendEntriesReply RaftNode::handleAppendEntries(const AppendEntriesArgs& args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    AppendEntriesReply reply;
    reply.term = m_currentTerm;
    reply.success = false;

    // 1. 如果 leader 任期小于当前任期，拒绝
    if (args.term < m_currentTerm) {
        return reply;
    }

    // 发现有效 leader，退回 Follower
    m_state = RaftState::Follower;
    m_currentTerm = args.term;
    resetElectionTimer(); // 收到合法心跳，重置超时

    // 2. 日志匹配检查
    if (args.prevLogIndex >= m_logs.size() || m_logs[args.prevLogIndex].term != args.prevLogTerm) {
        return reply;
    }

    // TODO: 3. 如果已经存在的日志条目和新的产生冲突（索引值相同但是任期号不同），删除这一条和之后所有的
    // TODO: 4. 附加任何在已有的日志中不存在的条目
    
    // 5. 更新提交索引
    if (args.leaderCommit > m_commitIndex) {
        m_commitIndex = std::min(args.leaderCommit, m_logs.back().index);
        // TODO: 将 commitIndex 应用到状态机(RocksDB)
    }

    reply.success = true;
    reply.term = m_currentTerm;
    return reply;
}

bool RaftNode::submitLog(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != RaftState::Leader) {
        return false;
    }

    LogEntry entry;
    entry.term = m_currentTerm;
    entry.index = m_logs.back().index + 1;
    entry.key = key;
    entry.value = value;
    
    m_logs.push_back(entry);
    LOG_INFO("Leader {} appended new log [index={}, term={}]", m_nodeId, entry.index, entry.term);
    
    // 下一次 heartbeatLoop 会把这条日志通过 AppendEntries 同步出去
    return true;
}

} // namespace raft
