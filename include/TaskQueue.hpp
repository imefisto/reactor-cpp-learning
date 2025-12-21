#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include "Task.hpp"

class TaskQueue
{
    public:
        void push(Task t);
        Task pop();
    private:
        std::queue<Task> queue_;
        std::mutex mtx_;
        std::condition_variable cv_;
};

#endif
