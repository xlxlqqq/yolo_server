#pragma once

#include <atomic>

namespace server {

class Server {
public:
    Server();
    ~Server();

    bool init();
    void start();
    void stop();
    void wait();

    bool isRunning() const;

private:
    std::atomic<bool> running_;
};

};  // namespace server