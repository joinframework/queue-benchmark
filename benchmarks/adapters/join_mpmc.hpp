#pragma once

#include <join/queue.hpp>
#include <string_view>

template <typename T>
class JoinMPMC
{
public:
    static constexpr std::string_view name = "join::LocalMem::Mpmc::Queue";

    explicit JoinMPMC (std::size_t capacity)
    : q_ (capacity)
    {
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
    join::LocalMem::Mpmc::Queue<T> q_;
};
