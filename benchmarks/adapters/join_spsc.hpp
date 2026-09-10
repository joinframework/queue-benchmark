#pragma once

#include <join/queue.hpp>
#include <string_view>

template <typename T>
class JoinSPSC
{
public:
    static constexpr std::string_view name = "join::LocalMem::Spsc::Queue";

    explicit JoinSPSC (std::size_t capacity)
    : _q (capacity)
    {
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
    join::LocalMem::Spsc::Queue<T> _q;
};
