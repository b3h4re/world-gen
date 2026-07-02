#include "thread_pool.hpp"

#include <algorithm>

namespace wgen {

std::size_t ThreadPool::defaultThreadCount() {
    return std::max(1U, std::thread::hardware_concurrency());
}

ThreadPool::ThreadPool(std::size_t threadCount) {
    workers_.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this] {
            workerLoop();
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock{mutex_};
        stopping_ = true;
    }
    condition_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock{mutex_};
            condition_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });
            if (stopping_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        task();
    }
}

} // namespace wgen
