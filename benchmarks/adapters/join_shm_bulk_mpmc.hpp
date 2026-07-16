#pragma once

#include "adapters/join_shm_name.hpp"
#include <join/queue.hpp>
#include <cstddef>
#include <string_view>

template <typename T>
class JoinShmBulkMPMC
{
public:
    static constexpr std::string_view name = "join::ShmMem::Mpmc::Queue";

    explicit JoinShmBulkMPMC (std::size_t cap)
    : name_ (next_join_shm_name ())
    , q_ (cap * sizeof (T) * 2, name_)
    {
    }

    ~JoinShmBulkMPMC ()
    {
        join::ShmMem::unlink (name_);
    }

    std::size_t push_bulk (const T* items, std::size_t count)
    {
        auto n = q_.tryPush (items, count);
        return n < 0 ? 0 : static_cast<std::size_t> (n);
    }

    std::size_t pop_bulk (T* buffer, std::size_t max_count)
    {
        auto n = q_.tryPop (buffer, max_count);
        return n < 0 ? 0 : static_cast<std::size_t> (n);
    }

private:
    std::string name_;
    join::ShmMem::Mpmc::Queue<T> q_;
};
