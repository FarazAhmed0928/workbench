// Fixed-size worker pool built on std::thread. The number of threads is
// bounded by the caller (and capped at the logical CPU count by the benchmark).
#pragma once
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace wb {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t workers);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    std::size_t size() const { return threads_.size(); }
    void submit(std::function<void()> task);
    void wait();  // blocks until every submitted task has finished

private:
    void run();
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex m_;
    std::condition_variable cvTask_, cvDone_;
    std::size_t active_ = 0;
    bool stop_ = false;
};

}  // namespace wb
