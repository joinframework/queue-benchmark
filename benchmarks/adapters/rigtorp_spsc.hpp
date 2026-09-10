#pragma once

#include <rigtorp/SPSCQueue.h>
#include <string_view>

template <typename T>
class RigtorpSPSC
{
public:
    static constexpr std::string_view name = "rigtorp::SPSCQueue";

    explicit RigtorpSPSC (std::size_t capacity)
    : _q (capacity)
    {
    }

    bool push (T val)
    {
        return _q.try_push (val);
    }

    bool pop (T& val)
    {
        T* ptr = _q.front ();
        if (!ptr)
            return false;
        val = *ptr;
        _q.pop ();
        return true;
    }

private:
    rigtorp::SPSCQueue<T> _q;
};
