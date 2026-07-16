#pragma once

#include <readerwriterqueue.h>
#include <string_view>

template <typename T>
class MoodycamelSPSC
{
public:
    static constexpr std::string_view name = "moodycamel::ReaderWriterQueue";

    explicit MoodycamelSPSC (std::size_t capacity)
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
    moodycamel::ReaderWriterQueue<T> q_;
};
