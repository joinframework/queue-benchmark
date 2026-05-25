#pragma once

#include <boost/lockfree/spsc_queue.hpp>
#include <string_view>

template<typename T>
class BoostSPSC
{
public:
    static constexpr std::string_view name = "boost::lockfree::spsc_queue";

    explicit BoostSPSC(std::size_t capacity) : q_(capacity) {}

    bool push(T val)  { return q_.push(val); }
    bool pop (T& val) { return q_.pop(val);  }

private:
    boost::lockfree::spsc_queue<T> q_;
};
