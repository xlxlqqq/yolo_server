#include "server/Connection.h"
#include "common/logger/Logger.h"
#include "server/Protocol.h"
#include "server/Dispatcher.h"
#include "server/Command.h"
#include "storage/YoloStorage.h"
#include "common/error/ErrorCode.h"

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <fcntl.h>


using json = nlohmann::json;

namespace server {

Connection::Connection(int fd, cluster::ShardRouter& router, const cluster::NodeInfo& self)
    : m_fd(fd),
        m_router(router),
        m_self(self) {}

Connection::~Connection() {
    if (m_fd >= 0) {
        LOG_INFO("connection fd {} closed", m_fd);
        close(m_fd);
    }
}

void Connection::start() {
    handle();
}

static std::string getFirstToken(const std::string& s) {
    std::istringstream iss(s);
    std::string token;
    iss >> token;
    return token;
}

void Connection::handle() {
    LOG_INFO("connection started, fd={}", m_fd);

    std::vector<char> data;
    
    while (Protocol::recvMessage(m_fd, data)) {
        std::string msg(data.begin(), data.end());

        LOG_INFO("received message: {}", msg);

        std::string cmd_str = getFirstToken(msg);
        CommandType cmd_type = parseCommand(cmd_str);

        switch (cmd_type) {
            case CommandType::PING:
                handlePing();
                break;
            case CommandType::STORE:
                handleStore(msg);
                break;
            case CommandType::GET:
                handleGet(msg);
                break;
            default:
                handleUnknown(msg);
                break;
        }
    }

    LOG_INFO("connection closed, fd={}", m_fd);
}

void Connection::handleUnknown(const std::string& msg) {
    LOG_WARN("unknown command: {}", msg);

    std::string reply = "ERROR unknown command";
    Protocol::sendMessage(m_fd,
        std::vector<char>(reply.begin(), reply.end()));
}

void Connection::handlePing() {
    std::string reply = "PONG";
    Protocol::sendMessage(m_fd,
        std::vector<char>(reply.begin(), reply.end()));
}


void Connection::handleStore(const std::string& msg) {
    LOG_INFO("STORE command received: {}", msg);

    // 1. 找到第一个空格
    auto pos = msg.find(' ');
    if (pos == std::string::npos) {
        LOG_WARN("STORE missing payload");
        return;
    }

    // 2. 提取 JSON 字符串
    std::string json_str = msg.substr(pos + 1);

    try {
        json j = json::parse(json_str);

        storage::YoloFrame frame;
        frame.image_id   = j.at("image_id").get<std::string>();
        frame.width      = j.at("width").get<int>();
        frame.height     = j.at("height").get<int>();
        frame.image_hash = j.at("image_hash").get<std::string>();

        for (const auto& jb : j.at("boxes")) {
            storage::YoloBox box;
            box.class_id  = jb.at("class_id").get<int>();
            box.x    = jb.at("x").get<float>();
            box.y    = jb.at("y").get<float>();
            box.w    = jb.at("w").get<float>();
            box.h    = jb.at("h").get<float>();
            box.confidence = jb.at("confidence").get<float>();
            frame.boxes.push_back(box);
        }

        const auto& node = m_router.pickNode(frame.image_id);
        LOG_INFO("self.id={}, picked node.id={}", m_self.id, node.id);

        if (node.id != m_self.id) {
            // 转发到目标节点处理 
            if(forwardToNode(node, msg) == CONNECT_FAILED) {
                LOG_ERROR("failed to forward STORE request to node {}, store failed", node.id);
                std::string reply = "ERROR failed to forward STORE request";
                Protocol::sendMessage(m_fd, std::vector<char>(reply.begin(), reply.end()));
                return;
            } else {
                LOG_INFO("STORE request forwarded to node {} successfully", node.id);
                return;
            }
        }
        
        // 否则直接存储到本地
        storage::YoloStorage::instance().store(frame);

        std::string reply = "STORE OK";
        Protocol::sendMessage(
            m_fd,
            std::vector<char>(reply.begin(), reply.end())
        );

        LOG_INFO("stored yolo frame: {}, boxes={}",
                 frame.image_id, frame.boxes.size());
    }
    catch (const std::exception& e) {
        LOG_WARN("STORE parse failed: {}", e.what());

        std::string reply = "ERROR invalid STORE payload";
        Protocol::sendMessage(
            m_fd,
            std::vector<char>(reply.begin(), reply.end())
        );
    }
}

void Connection::handleGet(const std::string& msg) const {
    LOG_INFO("GET command received: {}", msg);

    std::istringstream iss(msg);
    std::string cmd, image_id;
    iss >> cmd >> image_id;

    if (image_id.empty()) {
        std::string reply = "ERROR missing image_id";
        Protocol::sendMessage(m_fd,
            std::vector<char>(reply.begin(), reply.end()));
        return;
    }

    const auto& node = m_router.pickNode(image_id);
    LOG_INFO("self.id={}, picked node.id={}", m_self.id, node.id);

    if (node.id != m_self.id) {
        if(forwardToNode(node, msg) == CONNECT_FAILED) {
            LOG_ERROR("failed to forward GET request to node {}, store failed", node.id);
            std::string reply = "ERROR failed to forward GET request";
            Protocol::sendMessage(m_fd, std::vector<char>(reply.begin(), reply.end()));
        } else {
            LOG_INFO("GET request forwarded to node {} successfully", node.id);
            
        }
        return;
    }

    storage::YoloFrame frame;
    auto opt_frame = storage::YoloStorage::instance().get(image_id);
    if (!opt_frame.has_value()) {
        std::string reply = "ERROR not found";
        Protocol::sendMessage(m_fd,
            std::vector<char>(reply.begin(), reply.end()));
        return;
    }
    
    frame = opt_frame.value();
    // 构造 JSON
    json j;
    j["image_id"]   = frame.image_id;
    j["width"]      = frame.width;
    j["height"]     = frame.height;
    j["image_hash"] = frame.image_hash;

    j["boxes"] = json::array();
    for (const auto& box : frame.boxes) {
        j["boxes"].push_back({
            {"class_id", box.class_id},
            {"x", box.x},
            {"y", box.y},
            {"w", box.w},
            {"h", box.h},
            {"confidence", box.confidence}
        });
    }

    std::string reply = j.dump();
    Protocol::sendMessage(m_fd,
        std::vector<char>(reply.begin(), reply.end()));

    LOG_INFO("GET success: {}", image_id);
}

// TODO: 实际转发时需要建立到目标节点的连接
int Connection::forwardToNode(
    const cluster::NodeInfo& node, 
    const std::string& msg) const {
        
    LOG_INFO("forwarding request to {} ({}:{})",
             node.id, node.host, node.port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("socket creation failed");
        return FD_FIALED;
    }
    LOG_INFO("new socket fd: {}, local fd {}", fd, m_fd);

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    LOG_INFO("set socket fd {} non-blocking", fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(node.port);

    if (inet_pton(AF_INET, node.host.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("invalid node host: {}", node.host);
        close(fd);
        return FD_FIALED;
    }

    bool ret = server::Connection::connectWithRetry(fd, (sockaddr&)addr, node.host, node.port);
    // int ret = connect(fd, (sockaddr*)&addr, sizeof(addr));
    if (!ret) {
        LOG_ERROR("connect to {}:{} failed", node.host, node.port);
        close(fd);
        return CONNECT_FAILED;
    }

    LOG_INFO("now send msg: {} to {}", msg, node.id);
    Protocol::sendMessage(fd, std::vector<char>(msg.begin(), msg.end()));

    std::vector<char> reply;
    if (!Protocol::recvMessage(fd, reply)) {
        LOG_ERROR("failed to receive reply from node {}", node.id);
        close(fd);
        return RECV_FAILED;
    }

    close(fd);

    // 6. 回写给原客户端
    Protocol::sendMessage(m_fd, reply);

    LOG_INFO("request forwarded to node {} done", node.id);
    return OK;
}

bool server::Connection::connectWithRetry(int fd, const sockaddr& addr, const std::string& host, int port, int maxRetries, int retryDelayMs) {
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        if (connect(fd, &addr, sizeof(addr)) == 0) {
            // 连接成功
            return true;
        }

        // 记录错误日志
        LOG_ERROR("Attempt {}: connect to {}:{} failed", attempt, host, port);

        if (attempt < maxRetries) {
            // 等待一段时间后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        }
    }

    // 超过最大重试次数，返回失败
    LOG_ERROR("connect to {}:{} failed after {} attempts", host, port, maxRetries);
    return false;
}

};