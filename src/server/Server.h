#pragma once

#include <atomic>

namespace server {

class Server {
public:
    Server();
    ~Server();

    bool init();
    bool start();
    void stop();
    void wait();

    bool isRunning() const;

private:
    std::atomic<bool> m_running;

    bool setupSocket();
    void acceptLoop();

    int m_listen_fd{-1};
    int m_port{8080};
};

};  // namespace server