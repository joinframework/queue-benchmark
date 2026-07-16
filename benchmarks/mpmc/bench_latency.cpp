#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <numeric>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/boost_mpmc.hpp"
#include "adapters/join_mpmc.hpp"
#include "adapters/join_shm_mpmc.hpp"
#include "adapters/moodycamel_mpmc.hpp"
#include "adapters/rigtorp_mpmc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumLatencyItems = 100'000;

template <typename Adapter>
static void BM_MPMC_Latency (benchmark::State& state)
{
    const int num_producers = static_cast<int> (state.range (0));
    const int num_consumers = static_cast<int> (state.range (1));
    const std::size_t capacity = static_cast<std::size_t> (state.range (2));
    const std::size_t items_per_producer = kNumLatencyItems / static_cast<std::size_t> (num_producers);
    const std::size_t total_items = items_per_producer * static_cast<std::size_t> (num_producers);

    std::vector<std::vector<std::int64_t>> per_consumer_lats (num_consumers);
    for (auto& v : per_consumer_lats)
        v.reserve (total_items / static_cast<std::size_t> (num_consumers) + 1);

    for (auto _ : state)
    {
        for (auto& v : per_consumer_lats)
            v.clear ();

        Adapter q (capacity);
        std::atomic<std::size_t> total_consumed{0};
        const int total_threads = num_producers + num_consumers;
        std::barrier<> ready (total_threads + 1);
        std::barrier<> go (total_threads + 1);

        std::vector<std::thread> producers;
        producers.reserve (num_producers);
        for (int p = 0; p < num_producers; ++p)
        {
            producers.emplace_back ([&, p] {
                pin_bench_thread (p);
                ready.arrive_and_wait ();
                go.arrive_and_wait ();
                for (std::size_t i = 0; i < items_per_producer; ++i)
                {
                    std::int64_t ts = static_cast<std::int64_t> (rdtsc ());
                    while (!q.push (ts))
                        cpu_pause ();
                }
            });
        }

        std::vector<std::thread> consumers;
        consumers.reserve (num_consumers);
        for (int c = 0; c < num_consumers; ++c)
        {
            consumers.emplace_back ([&, c] {
                pin_bench_thread (num_producers + c);
                ready.arrive_and_wait ();
                go.arrive_and_wait ();
                std::int64_t ts;
                while (total_consumed.load (std::memory_order_relaxed) < total_items)
                {
                    if (q.pop (ts))
                    {
                        per_consumer_lats[c].push_back (cycles_to_ns (rdtsc () - static_cast<std::uint64_t> (ts)));
                        total_consumed.fetch_add (1, std::memory_order_relaxed);
                    }
                    else
                    {
                        cpu_pause ();
                    }
                }
            });
        }

        ready.arrive_and_wait ();
        go.arrive_and_wait ();

        for (auto& t : producers)
            t.join ();
        for (auto& t : consumers)
            t.join ();
    }

    std::vector<std::int64_t> all_latencies;
    all_latencies.reserve (total_items);
    for (auto& v : per_consumer_lats)
        all_latencies.insert (all_latencies.end (), v.begin (), v.end ());

    if (!all_latencies.empty ())
    {
        std::sort (all_latencies.begin (), all_latencies.end ());
        const auto n = static_cast<std::size_t> (all_latencies.size ());
        const double mean =
            static_cast<double> (std::accumulate (all_latencies.begin (), all_latencies.end (), std::int64_t{0})) / n;

        state.counters["lat_min_ns"] = benchmark::Counter (static_cast<double> (all_latencies.front ()));
        state.counters["lat_mean_ns"] = benchmark::Counter (mean);
        state.counters["lat_max_ns"] = benchmark::Counter (static_cast<double> (all_latencies.back ()));
        state.counters["lat_p50_ns"] = benchmark::Counter (static_cast<double> (all_latencies[n * 50 / 100]));
        state.counters["lat_p90_ns"] = benchmark::Counter (static_cast<double> (all_latencies[n * 90 / 100]));
        state.counters["lat_p99_ns"] = benchmark::Counter (static_cast<double> (all_latencies[n * 99 / 100]));
    }

    state.SetItemsProcessed (state.iterations () * static_cast<std::int64_t> (total_items));
    state.SetLabel (std::string (Adapter::name));
}

#define MPMC_LAT_ARGS         \
    ->Args ({1, 1, 4096})     \
        ->Args ({2, 2, 4096}) \
        ->Args ({4, 4, 4096}) \
        ->Args ({4, 1, 4096}) \
        ->Args ({1, 4, 4096}) \
        ->UseRealTime ()      \
        ->MinTime (0.5)

BENCHMARK (BM_MPMC_Latency<BoostMPMC<std::int64_t>>)->Name ("MPMC/Latency/Boost") MPMC_LAT_ARGS;

BENCHMARK (BM_MPMC_Latency<MoodycamelMPMC<std::int64_t>>)->Name ("MPMC/Latency/Moodycamel") MPMC_LAT_ARGS;

BENCHMARK (BM_MPMC_Latency<JoinMPMC<std::int64_t>>)->Name ("MPMC/Latency/Join (Local)") MPMC_LAT_ARGS;

BENCHMARK (BM_MPMC_Latency<JoinShmMPMC<std::int64_t>>)->Name ("MPMC/Latency/Join (Shm)") MPMC_LAT_ARGS;

BENCHMARK (BM_MPMC_Latency<RigtorpMPMC<std::int64_t>>)->Name ("MPMC/Latency/Rigtorp") MPMC_LAT_ARGS;

BENCHMARK_MAIN ();
