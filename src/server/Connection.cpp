#include "server/Connection.h"
#include "common/logger/Logger.h"
#include "server/Protocol.h"
#include "server/Dispatcher.h"
#include "server/Command.h"
#include "storage/YoloStorage.h"

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace server {

Connection::Connection(int fd) : m_fd(fd) {}

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

};