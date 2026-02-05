#pragma once

#include <netinet/in.h>

namespace server {

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    void start();  // 链接主逻辑

private:
    void handle();

private:
    int m_fd;

};

};  // namespace server