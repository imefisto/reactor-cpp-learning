#include "TaskQueue.hpp"

void TaskQueue::push(Task t)
{
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push(std::move(t));
    cv_.notify_one();
};

Task TaskQueue::pop()
{
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [&] { return !queue_.empty(); });
    Task t = queue_.front();
    queue_.pop();
    return std::move(t);
};
