#include "WorkerPool.hpp"

WorkerPool::WorkerPool(size_t n)
    : stop_(false)
{
    for (size_t i = 0; i < n; i++) {
        threads_.emplace_back([this] {
                loop();
                });
    }
};

WorkerPool::~WorkerPool()
{
    stop_ = true;
    // wake up all workers
    for (size_t i = 0; i < threads_.size(); i++) {
        queue_.push(Task{[]{}});
    }
    for (auto& t : threads_) {
        t.join();
    }
};

void WorkerPool::submit(Task t)
{
    queue_.push(std::move(t));
};

void WorkerPool::loop()
{
    while (!stop_) {
        Task t = queue_.pop();
        t.fn();
    }
};
