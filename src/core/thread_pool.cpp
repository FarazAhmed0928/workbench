#include "thread_pool.hpp"

#include <algorithm>

namespace wb {

ThreadPool::ThreadPool(std::size_t workers) {
    workers = std::max<std::size_t>(1, workers);
    for (std::size_t i = 0; i < workers; ++i) threads_.emplace_back([this] { run(); });
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lk(m_);
        stop_ = true;
    }
    cvTask_.notify_all();
    for (auto& t : threads_) t.join();
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lk(m_);
        tasks_.push(std::move(task));
    }
    cvTask_.notify_one();
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lk(m_);
    cvDone_.wait(lk, [this] { return tasks_.empty() && active_ == 0; });
}

void ThreadPool::run() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(m_);
            cvTask_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
            ++active_;
        }
        task();
        {
            std::lock_guard<std::mutex> lk(m_);
            --active_;
            if (tasks_.empty() && active_ == 0) cvDone_.notify_all();
        }
    }
}

}  // namespace wb
