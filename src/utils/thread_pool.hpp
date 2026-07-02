#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <future>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace wgen {

class ThreadPool {
public:
    static std::size_t defaultThreadCount();

    explicit ThreadPool(std::size_t threadCount = defaultThreadCount());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Func>
    auto submit(Func&& func) -> std::future<std::invoke_result_t<Func>> {
        using Result = std::invoke_result_t<Func>;

        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Func>(func));
        std::future<Result> future = task->get_future();
        {
            std::lock_guard lock{mutex_};
            tasks_.push_back([task] {
                (*task)();
            });
        }
        condition_.notify_one();
        return future;
    }

private:
    void workerLoop();

    std::vector<std::thread> workers_{};
    std::deque<std::function<void()>> tasks_{};
    std::mutex mutex_{};
    std::condition_variable condition_{};
    bool stopping_{false};
};

} // namespace wgen
