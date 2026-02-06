#pragma once

#include <netinet/in.h>
#include <string>

namespace server {

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    void start();  // 链接主逻辑

private:
    void handle();

    void handlePing();
    void handleUnknown(const std::string& msg);
    void handleStore(const std::string& msg);
    void handleGet(const std::string& msg);

private:
    int m_fd;

};

};  // namespace server