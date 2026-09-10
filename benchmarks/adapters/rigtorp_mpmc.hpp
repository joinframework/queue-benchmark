#pragma once

#include <rigtorp/MPMCQueue.h>
#include <string_view>

template <typename T>
class RigtorpMPMC
{
public:
    static constexpr std::string_view name = "rigtorp::MPMCQueue";

    explicit RigtorpMPMC (std::size_t capacity)
    : _q (capacity)
    {
    }

    bool push (T val)
    {
        return _q.try_push (val);
    }

    bool pop (T& val)
    {
        return _q.try_pop (val);
    }

private:
    rigtorp::MPMCQueue<T> _q;
};
