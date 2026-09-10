#pragma once

#include <concurrentqueue.h>
#include <string_view>

template <typename T>
class MoodycamelMPMC
{
public:
    static constexpr std::string_view name = "moodycamel::ConcurrentQueue";

    explicit MoodycamelMPMC (std::size_t capacity)
    : _q (capacity)
    {
    }

    bool push (T val)
    {
        return _q.try_enqueue (val);
    }

    bool pop (T& val)
    {
        return _q.try_dequeue (val);
    }

private:
    moodycamel::ConcurrentQueue<T> _q;
};
