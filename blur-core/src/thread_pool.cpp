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

void ThreadPool::parallelFor(int total, int chunkSize,
                              const std::function<void(int, int)>& fn) {
    if (total <= 0) return;
    if (chunkSize <= 0) chunkSize = 1;

    std::atomic<int> nextIndex{0};
    int numThreads = static_cast<int>(workers.size());

    std::vector<std::future<void>> futures;
    futures.reserve(static_cast<size_t>(numThreads));

    for (int t = 0; t < numThreads; ++t) {
        futures.emplace_back(enqueue([&] {
            while (true) {
                int start = nextIndex.fetch_add(chunkSize);
                if (start >= total) break;
                int end = std::min(start + chunkSize, total);
                fn(start, end);
            }
        }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

} // namespace blur
