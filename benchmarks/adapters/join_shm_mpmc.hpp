#pragma once

#include "adapters/join_shm_name.hpp"
#include <join/queue.hpp>
#include <string_view>

template <typename T>
class JoinShmMPMC
{
public:
    static constexpr std::string_view name = "join::ShmMem::Mpmc::Queue";

    explicit JoinShmMPMC (std::size_t capacity)
    : name_ (next_join_shm_name ())
    , q_ (capacity, name_)
    {
    }

    ~JoinShmMPMC ()
    {
        join::ShmMem::unlink (name_);
    }

    bool push (T val)
    {
        return q_.tryPush (val) == 0;
    }

    bool pop (T& val)
    {
        return q_.tryPop (val) == 0;
    }

private:
    std::string name_;
    join::ShmMem::Mpmc::Queue<T> q_;
};
