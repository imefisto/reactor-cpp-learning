#ifndef TIMER_H
#define TIMER_H

struct Timer 
{
    int id;
    uint64_t expiresAt;
    uint64_t interval;
    std::function<void()> callback;
};

#endif
