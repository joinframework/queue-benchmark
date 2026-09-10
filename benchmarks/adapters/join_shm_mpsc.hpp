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
    : _name (next_join_shm_name ())
    , _q (capacity, _name)
    {
    }

    ~JoinShmMPSC ()
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
    join::ShmMem::Mpsc::Queue<T> _q;
};
