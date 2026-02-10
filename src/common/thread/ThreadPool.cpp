#include "common/thread/ThreadPool.h"

namespace common {

ThreadPool::ThreadPool(size_t thread_count) {
    if (thread_count == 0) {
        thread_count = 1;
    }

    m_workers.reserve(thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        m_workers.emplace_back([this]() {
            workerLoop();
        });
    }
}

ThreadPool::~ThreadPool() {
    m_stopping.store(true);

    m_cv.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::submit(std::function<void()> task) {
    if (m_stopping.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.emplace(std::move(task));
    }

    m_cv.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() {
                return m_stopping.load() || !m_tasks.empty();
            });

            if (m_stopping.load() && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        // 执行任务
        try {
            task();
        } catch (...) {
            // ⚠️ 防止 worker 因异常退出
            // 这里你可以加 LOG_ERROR
        }
    }
}

} // namespace common
