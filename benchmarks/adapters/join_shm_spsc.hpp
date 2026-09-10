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
    : _name (next_join_shm_name ())
    , _q (capacity, _name)
    {
    }

    ~JoinShmSPSC ()
    {
        join::ShmMem::unlink (_name);
    }

    bool push (T val)
    {
        return _q.tryPush (val) == 0;
    }

    bool pop (T& val)
    {
        return _q.tryPop (val) == 0;
    }

private:
    std::string _name;
    join::ShmMem::Spsc::Queue<T> _q;
};
