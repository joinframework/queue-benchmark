#pragma once

#include <atomic>
#include <string>
#include <unistd.h>

inline std::string next_join_shm_name ()
{
    static std::atomic<unsigned long> counter{0};
    return "/queue-bench-" + std::to_string (::getpid ()) + "-" +
           std::to_string (counter.fetch_add (1, std::memory_order_relaxed));
}
