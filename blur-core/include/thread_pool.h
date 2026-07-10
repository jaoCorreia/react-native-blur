#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <stdexcept>

namespace blur {

class ThreadPool {
public:
    static ThreadPool& instance();
    static void shutdown();

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    int threadCount() const;

    void parallelFor(int total, int chunkSize,
                     const std::function<void(int, int)>& fn);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    explicit ThreadPool(size_t numThreads = 0);

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
};

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return result;
}

} // namespace blur
