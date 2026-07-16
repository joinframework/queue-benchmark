#pragma once

#include <concurrentqueue.h>
#include <string_view>

template <typename T>
class MoodycamelMPSC
{
public:
    static constexpr std::string_view name = "moodycamel::ConcurrentQueue";

    explicit MoodycamelMPSC (std::size_t capacity)
    : q_ (capacity)
    {
    }

    bool push (T val)
    {
        return q_.try_enqueue (val);
    }

    bool pop (T& val)
    {
        return q_.try_dequeue (val);
    }

private:
    moodycamel::ConcurrentQueue<T> q_;
};
