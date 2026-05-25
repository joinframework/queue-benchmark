#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <pthread.h>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

inline void pin_thread(int cpu_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0)
        throw std::runtime_error("pin_thread: failed for cpu " + std::to_string(cpu_id));
}

inline std::vector<int> physical_cores()
{
    std::vector<int> result;
    const int ncpus = static_cast<int>(std::thread::hardware_concurrency());
    std::set<std::string> seen;
    for (int i = 0; i < ncpus; ++i)
    {
        std::string path = "/sys/devices/system/cpu/cpu"
                         + std::to_string(i)
                         + "/topology/thread_siblings_list";
        std::ifstream f(path);
        if (!f.is_open()) {
            result.resize(static_cast<std::size_t>(ncpus));
            std::iota(result.begin(), result.end(), 0);
            return result;
        }
        std::string siblings;
        std::getline(f, siblings);
        if (seen.insert(siblings).second)
            result.push_back(i);
    }
    return result;
}

inline void pin_bench_thread(int idx) noexcept
{
    static const std::vector<int> cores = physical_cores();
    if (idx >= 0 && static_cast<std::size_t>(idx) < cores.size())
        pin_thread(cores[static_cast<std::size_t>(idx)]);
}

inline void cpu_pause() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#else
    std::this_thread::yield();
#endif
}

namespace detail {
inline double calibrate_tsc() noexcept
{
    using clk = std::chrono::high_resolution_clock;
    auto t0 = clk::now();
    asm volatile("" ::: "memory");
    std::uint64_t c0 = __builtin_ia32_rdtsc();
    auto deadline = t0 + std::chrono::milliseconds(20);
    while (clk::now() < deadline) {}
    asm volatile("" ::: "memory");
    std::uint64_t c1 = __builtin_ia32_rdtsc();
    auto t1 = clk::now();
    double ns = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    return static_cast<double>(c1 - c0) / ns;
}
}

namespace detail {
inline const double g_tsc_ghz = calibrate_tsc();
}

inline double tsc_ghz() noexcept { return detail::g_tsc_ghz; }

inline std::uint64_t rdtsc() noexcept
{
    asm volatile("" ::: "memory");
    return __builtin_ia32_rdtsc();
}

inline std::int64_t cycles_to_ns(std::uint64_t cycles) noexcept
{
    return static_cast<std::int64_t>(static_cast<double>(cycles) / tsc_ghz());
}

inline std::int64_t now_ns() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}
