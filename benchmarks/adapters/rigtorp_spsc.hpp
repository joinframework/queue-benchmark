#pragma once

#include <rigtorp/SPSCQueue.h>
#include <string_view>

template <typename T>
class RigtorpSPSC
{
public:
    static constexpr std::string_view name = "rigtorp::SPSCQueue";

    explicit RigtorpSPSC (std::size_t capacity)
    : q_ (capacity)
    {
    }

    bool push (T val)
    {
        return q_.try_push (val);
    }

    bool pop (T& val)
    {
        T* ptr = q_.front ();
        if (!ptr)
            return false;
        val = *ptr;
        q_.pop ();
        return true;
    }

private:
    rigtorp::SPSCQueue<T> q_;
};
