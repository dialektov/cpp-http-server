#include "thread_pool.hpp"

#include <stdexcept>

ThreadPool::ThreadPool(std::size_t worker_count) {
    if (worker_count == 0) {
        throw std::invalid_argument("thread pool size must be greater than zero");
    }

    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock lock(mutex_);
                    cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard lock(mutex_);
        if (stop_) {
            throw std::runtime_error("cannot enqueue into stopped thread pool");
        }
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}
