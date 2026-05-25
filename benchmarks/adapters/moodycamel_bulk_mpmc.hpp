#pragma once

#include <concurrentqueue.h>
#include <cstddef>
#include <string_view>

template<typename T>
class MoodycamelBulkMPMC
{
public:
    static constexpr std::string_view name = "moodycamel::ConcurrentQueue";

    explicit MoodycamelBulkMPMC(std::size_t cap)
        : q_(cap) {}

    std::size_t push_bulk(const T* items, std::size_t count) {
        return q_.enqueue_bulk(items, count) ? count : 0;
    }

    std::size_t pop_bulk(T* buffer, std::size_t max_count) {
        return q_.try_dequeue_bulk(buffer, max_count);
    }

private:
    moodycamel::ConcurrentQueue<T> q_;
};
