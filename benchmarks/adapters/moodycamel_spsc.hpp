#pragma once

#include <readerwriterqueue.h>
#include <string_view>

template <typename T>
class MoodycamelSPSC
{
public:
    static constexpr std::string_view name = "moodycamel::ReaderWriterQueue";

    explicit MoodycamelSPSC (std::size_t capacity)
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
    moodycamel::ReaderWriterQueue<T> _q;
};
