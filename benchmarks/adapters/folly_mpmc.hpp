#pragma once

#include <folly/MPMCQueue.h>
#include <cstddef>
#include <string_view>

template<typename T>
class FollyMPMC
{
public:
    static constexpr std::string_view name = "folly::MPMCQueue";

    explicit FollyMPMC(std::size_t capacity) : q_(capacity) {}

    bool push(T val)  { return q_.writeIfNotFull(std::move(val)); }
    bool pop (T& val) { return q_.readIfNotEmpty(val); }

private:
    folly::MPMCQueue<T> q_;
};
