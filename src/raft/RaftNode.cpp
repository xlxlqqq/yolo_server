#include "RaftNode.h"
#include "common/logger/Logger.h"
#include "server/Protocol.h"

#include <random>
#include <chrono>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

using json = nlohmann::json;

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

void RaftNode::setPeerAddress(const std::string& peerId, const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_peerAddresses[peerId] = std::make_pair(host, port);
    LOG_INFO("Set peer {} address: {}:{}", peerId, host, port);
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

    // 发送投票请求并统计选票
    int votes = 1; // 自己的一票
    for (const auto& peer : m_peers) {
        RequestVoteReply reply;
        if (sendRequestVote(peer, args, reply)) {
            if (reply.voteGranted) {
                votes++;
                LOG_INFO("Node {} received vote from {}", m_nodeId, peer);
            } else if (reply.term > m_currentTerm) {
                // 发现更大的任期，退回 Follower
                m_currentTerm = reply.term;
                m_state = RaftState::Follower;
                m_votedFor = "";
                LOG_INFO("Node {} found higher term {}, becoming follower", m_nodeId, reply.term);
                return;
            }
        }
    }

    // 检查是否获得多数选票
    int majority = (m_peers.size() + 1) / 2 + 1; // +1 包括自己
    if (votes >= majority) {
        // 成为 Leader
        m_state = RaftState::Leader;
        LOG_INFO("Node {} elected as leader for term {}", m_nodeId, m_currentTerm);
        
        // 初始化 nextIndex 和 matchIndex
        for (const auto& peer : m_peers) {
            m_nextIndex[peer] = m_logs.size();
            m_matchIndex[peer] = 0;
        }
        
        // 立即发送一次空日志作为心跳
        // 心跳会在 heartbeatLoop 中处理
    }
}

void RaftNode::heartbeatLoop() {
    while (m_running) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_state == RaftState::Leader) {
                // 发送心跳和日志给所有 Follower
                for (const auto& peer : m_peers) {
                    AppendEntriesArgs args;
                    args.term = m_currentTerm;
                    args.leaderId = m_nodeId;
                    
                    // 确定 prevLogIndex 和 prevLogTerm
                    uint64_t nextIndex = m_nextIndex[peer];
                    args.prevLogIndex = nextIndex - 1;
                    args.prevLogTerm = m_logs[args.prevLogIndex].term;
                    
                    // 准备要发送的日志条目
                    std::vector<LogEntry> entries;
                    for (uint64_t i = nextIndex; i < m_logs.size(); i++) {
                        entries.push_back(m_logs[i]);
                    }
                    
                    args.leaderCommit = m_commitIndex;
                    
                    AppendEntriesReply reply;
                    if (sendAppendEntries(peer, args, reply)) {
                        if (reply.success) {
                            // 成功，更新 nextIndex 和 matchIndex
                            m_nextIndex[peer] = nextIndex + entries.size();
                            m_matchIndex[peer] = m_nextIndex[peer] - 1;
                            
                            // 检查是否有新的日志可以提交
                            // 找到大多数节点都已复制的最高日志索引
                            std::vector<uint64_t> matchIndices;
                            matchIndices.push_back(m_logs.size() - 1); // 自己的
                            for (const auto& [p, mi] : m_matchIndex) {
                                matchIndices.push_back(mi);
                            }
                            std::sort(matchIndices.begin(), matchIndices.end(), std::greater<uint64_t>());
                            uint64_t newCommitIndex = matchIndices[(matchIndices.size() - 1) / 2];
                            
                            if (newCommitIndex > m_commitIndex && m_logs[newCommitIndex].term == m_currentTerm) {
                                m_commitIndex = newCommitIndex;
                                LOG_INFO("Leader {} updated commitIndex to {}", m_nodeId, m_commitIndex);
                                // TODO: 将 commitIndex 应用到状态机(RocksDB)
                            }
                        } else if (reply.term > m_currentTerm) {
                            // 发现更大的任期，退回 Follower
                            m_currentTerm = reply.term;
                            m_state = RaftState::Follower;
                            m_votedFor = "";
                            LOG_INFO("Leader {} found higher term {}, becoming follower", m_nodeId, reply.term);
                            break;
                        } else {
                            // 日志不匹配，回退 nextIndex 并重试
                            if (m_nextIndex[peer] > 1) {
                                m_nextIndex[peer]--;
                                LOG_DEBUG("Leader {} decremented nextIndex for {} to {}", m_nodeId, peer, m_nextIndex[peer]);
                            }
                        }
                    }
                }
            }
        }
        // 心跳间隔通常小于选举超时下限，比如 50ms
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

bool RaftNode::sendRequestVote(const std::string& peerId, const RequestVoteArgs& args, RequestVoteReply& reply) {
    auto it = m_peerAddresses.find(peerId);
    if (it == m_peerAddresses.end()) {
        LOG_WARN("No address for peer {}", peerId);
        return false;
    }
    
    const auto& [host, port] = it->second;
    
    // 创建 socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("socket creation failed");
        return false;
    }
    
    // 设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 连接到 peer
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("invalid peer host: {}", host);
        close(fd);
        return false;
    }
    
    // 连接（带重试）
    bool connected = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!connected) {
        LOG_ERROR("connect to {}:{} failed", host, port);
        close(fd);
        return false;
    }
    
    // 构建请求消息
    json j_args = {
        {"term", args.term},
        {"candidateId", args.candidateId},
        {"lastLogIndex", args.lastLogIndex},
        {"lastLogTerm", args.lastLogTerm}
    };
    
    std::string msg = "RAFT_REQUEST_VOTE " + j_args.dump();
    
    // 发送消息
    if (!server::Protocol::sendMessage(fd, std::vector<char>(msg.begin(), msg.end()))) {
        LOG_ERROR("failed to send RequestVote to {}", peerId);
        close(fd);
        return false;
    }
    
    // 接收响应
    std::vector<char> response;
    if (!server::Protocol::recvMessage(fd, response)) {
        LOG_ERROR("failed to receive RequestVoteReply from {}", peerId);
        close(fd);
        return false;
    }
    
    close(fd);
    
    // 解析响应
    std::string reply_str(response.begin(), response.end());
    size_t space_pos = reply_str.find(' ');
    if (space_pos == std::string::npos) {
        LOG_ERROR("invalid RequestVoteReply format");
        return false;
    }
    
    std::string reply_payload = reply_str.substr(space_pos + 1);
    try {
        json j_reply = json::parse(reply_payload);
        reply.term = j_reply["term"];
        reply.voteGranted = j_reply["voteGranted"];
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("failed to parse RequestVoteReply: {}", e.what());
        return false;
    }
}

bool RaftNode::sendAppendEntries(const std::string& peerId, const AppendEntriesArgs& args, AppendEntriesReply& reply) {
    auto it = m_peerAddresses.find(peerId);
    if (it == m_peerAddresses.end()) {
        LOG_WARN("No address for peer {}", peerId);
        return false;
    }
    
    const auto& [host, port] = it->second;
    
    // 创建 socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("socket creation failed");
        return false;
    }
    
    // 设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 连接到 peer
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("invalid peer host: {}", host);
        close(fd);
        return false;
    }
    
    // 连接（带重试）
    bool connected = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!connected) {
        LOG_ERROR("connect to {}:{} failed", host, port);
        close(fd);
        return false;
    }
    
    // 构建请求消息
    json j_args = {
        {"term", args.term},
        {"leaderId", args.leaderId},
        {"prevLogIndex", args.prevLogIndex},
        {"prevLogTerm", args.prevLogTerm},
        {"leaderCommit", args.leaderCommit},
        {"entries", json::array()}
    };
    
    // 添加日志条目
    for (const auto& entry : args.entries) {
        json j_entry = {
            {"term", entry.term},
            {"index", entry.index},
            {"key", entry.key},
            {"value", entry.value}
        };
        j_args["entries"].push_back(j_entry);
    }
    
    std::string msg = "RAFT_APPEND_ENTRIES " + j_args.dump();
    
    // 发送消息
    if (!server::Protocol::sendMessage(fd, std::vector<char>(msg.begin(), msg.end()))) {
        LOG_ERROR("failed to send AppendEntries to {}", peerId);
        close(fd);
        return false;
    }
    
    // 接收响应
    std::vector<char> response;
    if (!server::Protocol::recvMessage(fd, response)) {
        LOG_ERROR("failed to receive AppendEntriesReply from {}", peerId);
        close(fd);
        return false;
    }
    
    close(fd);
    
    // 解析响应
    std::string reply_str(response.begin(), response.end());
    size_t space_pos = reply_str.find(' ');
    if (space_pos == std::string::npos) {
        LOG_ERROR("invalid AppendEntriesReply format");
        return false;
    }
    
    std::string reply_payload = reply_str.substr(space_pos + 1);
    try {
        json j_reply = json::parse(reply_payload);
        reply.term = j_reply["term"];
        reply.success = j_reply["success"];
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("failed to parse AppendEntriesReply: {}", e.what());
        return false;
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

    // 3. 如果已经存在的日志条目和新的产生冲突（索引值相同但是任期号不同），删除这一条和之后所有的
    size_t nextIndex = args.prevLogIndex + 1;
    size_t entryIndex = 0;
    
    while (nextIndex < m_logs.size() && entryIndex < args.entries.size()) {
        if (m_logs[nextIndex].term != args.entries[entryIndex].term) {
            // 发现冲突，删除当前及之后的所有日志
            LOG_INFO("Log conflict detected at index {}, deleting {} entries", nextIndex, m_logs.size() - nextIndex);
            m_logs.erase(m_logs.begin() + nextIndex, m_logs.end());
            break;
        }
        nextIndex++;
        entryIndex++;
    }

    // 4. 附加任何在已有的日志中不存在的条目
    if (entryIndex < args.entries.size()) {
        for (size_t i = entryIndex; i < args.entries.size(); i++) {
            const auto& entry = args.entries[i];
            m_logs.push_back(entry);
            LOG_INFO("Appended log entry [index={}, term={}, key={}]", entry.index, entry.term, entry.key);
        }
    }

    // 5. 更新提交索引
    if (args.leaderCommit > m_commitIndex) {
        m_commitIndex = std::min(args.leaderCommit, m_logs.back().index);
        LOG_INFO("Updated commitIndex to {}", m_commitIndex);
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
