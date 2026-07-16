#pragma once

#include <boost/lockfree/queue.hpp>
#include <string_view>

template <typename T>
class BoostMPMC
{
public:
    static constexpr std::string_view name = "boost::lockfree::queue";

    explicit BoostMPMC (std::size_t capacity)
    : q_ (capacity)
    {
    }

    bool push (T val)
    {
        return q_.push (val);
    }

    bool pop (T& val)
    {
        return q_.pop (val);
    }

private:
    boost::lockfree::queue<T, boost::lockfree::fixed_sized<true>> q_;
};
