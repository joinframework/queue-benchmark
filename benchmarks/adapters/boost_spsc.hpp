#pragma once

#include <boost/lockfree/spsc_queue.hpp>
#include <string_view>

template <typename T>
class BoostSPSC
{
public:
    static constexpr std::string_view name = "boost::lockfree::spsc_queue";

    explicit BoostSPSC (std::size_t capacity)
    : _q (capacity)
    {
    }

    bool push (T val)
    {
        return _q.push (val);
    }

    bool pop (T& val)
    {
        return _q.pop (val);
    }

private:
    boost::lockfree::spsc_queue<T> _q;
};
