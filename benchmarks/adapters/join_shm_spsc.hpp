#pragma once

#include "adapters/join_shm_name.hpp"
#include <join/queue.hpp>
#include <string_view>

template <typename T>
class JoinShmSPSC
{
public:
    static constexpr std::string_view name = "join::ShmMem::Spsc::Queue";

    explicit JoinShmSPSC (std::size_t capacity)
    : name_ (next_join_shm_name ())
    , q_ (capacity, name_)
    {
    }

    ~JoinShmSPSC ()
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
    join::ShmMem::Spsc::Queue<T> q_;
};
