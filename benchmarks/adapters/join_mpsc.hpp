#pragma once

#include <join/queue.hpp>
#include <string_view>

template <typename T>
class JoinMPSC
{
public:
    static constexpr std::string_view name = "join::LocalMem::Mpsc::Queue";

    explicit JoinMPSC (std::size_t capacity)
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
    join::LocalMem::Mpsc::Queue<T> _q;
};
