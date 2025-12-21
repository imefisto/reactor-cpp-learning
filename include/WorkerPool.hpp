#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <thread>
#include "Task.hpp"
#include "TaskQueue.hpp"

class WorkerPool
{
    public:
        WorkerPool(size_t n);
        ~WorkerPool();
        void submit(Task t);
    private:
        void loop();
        std::vector<std::thread> threads_;
        TaskQueue queue_;
        std::atomic<bool> stop_;
};

#endif
