#pragma once

#include "adapters/join_shm_name.hpp"
#include <join/queue.hpp>
#include <cstddef>
#include <string_view>

template <typename T>
class JoinShmBulkSPSC
{
public:
    static constexpr std::string_view name = "join::ShmMem::Spsc::Queue";

    explicit JoinShmBulkSPSC (std::size_t cap)
    : _name (next_join_shm_name ())
    , _q (cap * sizeof (T) * 2, _name)
    {
    }

    ~JoinShmBulkSPSC ()
    {
        join::ShmMem::unlink (_name);
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
    std::string _name;
    join::ShmMem::Spsc::Queue<T> _q;
};
