#pragma once

#include <concurrentqueue.h>
#include <cstddef>
#include <string_view>

template <typename T>
class MoodycamelBulkMPSC
{
public:
    static constexpr std::string_view name = "moodycamel::ConcurrentQueue";

    explicit MoodycamelBulkMPSC (std::size_t cap)
    : _q (cap)
    {
    }

    std::size_t push_bulk (const T* items, std::size_t count)
    {
        return _q.try_enqueue_bulk (items, count) ? count : 0;
    }

    std::size_t pop_bulk (T* buffer, std::size_t max_count)
    {
        return _q.try_dequeue_bulk (buffer, max_count);
    }

private:
    moodycamel::ConcurrentQueue<T> _q;
};
