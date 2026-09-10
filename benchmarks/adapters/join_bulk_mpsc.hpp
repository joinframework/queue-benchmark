#pragma once

#include <join/queue.hpp>
#include <cstddef>
#include <string_view>

template <typename T>
class JoinBulkMPSC
{
public:
    static constexpr std::string_view name = "join::LocalMem::Mpsc::Queue";

    explicit JoinBulkMPSC (std::size_t cap)
    : _q (cap * sizeof (T) * 2)
    {
    }

    std::size_t push_bulk (const T* items, std::size_t count)
    {
        auto n = _q.tryPush (items, count);
        return n < 0 ? 0 : static_cast<std::size_t> (n);
    }

    std::size_t pop_bulk (T* buffer, std::size_t max_count)
    {
        auto n = _q.tryPop (buffer, max_count);
        return n < 0 ? 0 : static_cast<std::size_t> (n);
    }

private:
    join::LocalMem::Mpsc::Queue<T> _q;
};
