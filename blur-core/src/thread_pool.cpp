#include "thread_pool.h"
#include <algorithm>

namespace blur {

ThreadPool& ThreadPool::instance() {
    static ThreadPool pool(std::max(1u, std::thread::hardware_concurrency()));
    return pool;
}

void ThreadPool::shutdown() {
    instance().stop = true;
    instance().condition.notify_all();
    for (auto& worker : instance().workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

ThreadPool::ThreadPool(size_t numThreads) {
    if (numThreads == 0) {
        numThreads = std::max(1u, std::thread::hardware_concurrency());
    }

    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] {
                        return stop.load() || !tasks.empty();
                    });

                    if (stop && tasks.empty()) {
                        return;
                    }

                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

int ThreadPool::threadCount() const {
    return static_cast<int>(workers.size());
}

} // namespace blur
