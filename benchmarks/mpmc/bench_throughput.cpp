#include <atomic>
#include <barrier>
#include <chrono>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/boost_mpmc.hpp"
#include "adapters/join_mpmc.hpp"
#include "adapters/moodycamel_mpmc.hpp"
#include "adapters/rigtorp_mpmc.hpp"
#ifdef HAVE_FOLLY
#include "adapters/folly_mpmc.hpp"
#endif
#include "common/utils.hpp"

static constexpr std::size_t kNumItems = 1'000'000;

template<typename Adapter>
static void BM_MPMC_Throughput(benchmark::State& state)
{
    const int         num_producers      = static_cast<int>(state.range(0));
    const int         num_consumers      = static_cast<int>(state.range(1));
    const std::size_t capacity           = static_cast<std::size_t>(state.range(2));
    const std::size_t items_per_producer = kNumItems / static_cast<std::size_t>(num_producers);
    const std::size_t total_items        = items_per_producer * static_cast<std::size_t>(num_producers);

    for (auto _ : state) {
        Adapter q(capacity);
        std::atomic<std::size_t> total_consumed{0};
        const int total_threads = num_producers + num_consumers;
        std::barrier<> ready(total_threads + 1);
        std::barrier<> go   (total_threads + 1);

        std::vector<std::thread> producers;
        producers.reserve(num_producers);
        for (int p = 0; p < num_producers; ++p) {
            producers.emplace_back([&, p] {
                pin_bench_thread(p);
                ready.arrive_and_wait();
                go.arrive_and_wait();
                const std::size_t start = static_cast<std::size_t>(p) * items_per_producer;
                for (std::size_t i = start; i < start + items_per_producer; ++i)
                    while (!q.push(static_cast<int>(i))) cpu_pause();
            });
        }

        std::vector<std::thread> consumers;
        consumers.reserve(num_consumers);
        for (int c = 0; c < num_consumers; ++c) {
            consumers.emplace_back([&, c] {
                pin_bench_thread(num_producers + c);
                ready.arrive_and_wait();
                go.arrive_and_wait();
                int val;
                while (total_consumed.load(std::memory_order_relaxed) < total_items) {
                    if (q.pop(val))
                        total_consumed.fetch_add(1, std::memory_order_relaxed);
                    else
                        cpu_pause();
                }
            });
        }

        ready.arrive_and_wait();
        go.arrive_and_wait();

        auto t0 = std::chrono::high_resolution_clock::now();
        for (auto& t : producers) t.join();
        for (auto& t : consumers) t.join();
        auto t1 = std::chrono::high_resolution_clock::now();

        state.SetIterationTime(std::chrono::duration<double>(t1 - t0).count());
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kNumItems));
    state.SetLabel(std::string(Adapter::name));
}

#define MPMC_ARGS \
    ->Args({1, 1, 4096}) \
    ->Args({2, 2, 4096}) \
    ->Args({4, 4, 4096}) \
    ->Args({8, 8, 4096}) \
    ->Args({4, 1, 4096}) \
    ->Args({1, 4, 4096}) \
    ->UseManualTime()

BENCHMARK(BM_MPMC_Throughput<BoostMPMC<int>>)
    ->Name("MPMC/Throughput/Boost")
    MPMC_ARGS;

BENCHMARK(BM_MPMC_Throughput<MoodycamelMPMC<int>>)
    ->Name("MPMC/Throughput/Moodycamel")
    MPMC_ARGS;

BENCHMARK(BM_MPMC_Throughput<JoinMPMC<int>>)
    ->Name("MPMC/Throughput/Join")
    MPMC_ARGS;

BENCHMARK(BM_MPMC_Throughput<RigtorpMPMC<int>>)
    ->Name("MPMC/Throughput/Rigtorp")
    MPMC_ARGS;

#ifdef HAVE_FOLLY
BENCHMARK(BM_MPMC_Throughput<FollyMPMC<int>>)
    ->Name("MPMC/Throughput/Folly")
    MPMC_ARGS;
#endif

BENCHMARK_MAIN();
