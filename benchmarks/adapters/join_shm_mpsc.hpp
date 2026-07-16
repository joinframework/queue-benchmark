#pragma once

#include "adapters/join_shm_name.hpp"
#include <join/queue.hpp>
#include <string_view>

template <typename T>
class JoinShmMPSC
{
public:
    static constexpr std::string_view name = "join::ShmMem::Mpsc::Queue";

    explicit JoinShmMPSC (std::size_t capacity)
    : name_ (next_join_shm_name ())
    , q_ (capacity, name_)
    {
    }

    ~JoinShmMPSC ()
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
    join::ShmMem::Mpsc::Queue<T> q_;
};
