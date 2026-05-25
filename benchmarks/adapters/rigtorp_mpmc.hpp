#pragma once

#include <rigtorp/MPMCQueue.h>
#include <string_view>

template<typename T>
class RigtorpMPMC
{
public:
    static constexpr std::string_view name = "rigtorp::MPMCQueue";

    explicit RigtorpMPMC(std::size_t capacity) : q_(capacity) {}

    bool push(T val)  { return q_.try_push(val); }
    bool pop (T& val) { return q_.try_pop(val);  }

private:
    rigtorp::MPMCQueue<T> q_;
};
