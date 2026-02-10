#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace common {

class ThreadPool {
public:
    explicit ThreadPool(size_t thread_count);
    ~ThreadPool();

    // 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交任务
    void submit(std::function<void()> task);

    size_t size() const { return m_workers.size(); }

private:
    void workerLoop();

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    std::atomic<bool> m_stopping{false};
};

} // namespace common
