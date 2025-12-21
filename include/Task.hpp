#ifndef TASK_H
#define TASK_H

struct Task
{
    std::function<void()> fn;
};
#endif
