#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <numeric>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/join_mpsc.hpp"
#include "adapters/join_shm_mpsc.hpp"
#include "adapters/moodycamel_mpsc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumLatencyItems = 50'000;

static constexpr double kOfferedItemsPerSec = 1'000'000.0;

template <typename Adapter>
static void BM_MPSC_Latency (benchmark::State& state)
{
    const int num_producers = static_cast<int> (state.range (0));
    const int nthreads = num_producers + 1;
    if (nthreads > static_cast<int> (bench_cpus ().size ()))
    {
        state.SkipWithError ("not enough logical CPUs");
        return;
    }

    const std::size_t capacity = static_cast<std::size_t> (state.range (1));
    const std::size_t items_per_producer = kNumLatencyItems / static_cast<std::size_t> (num_producers);
    const std::size_t total_items = items_per_producer * static_cast<std::size_t> (num_producers);

    const std::uint64_t period = static_cast<std::uint64_t> (tsc_ghz () * 1e9 / kOfferedItemsPerSec * num_producers);

    std::vector<std::int64_t> latencies;
    latencies.reserve (total_items);

    std::size_t last_dropped = 0;

    for (auto _ : state)
    {
        latencies.clear ();

        Adapter q (capacity);
        std::atomic<std::size_t> dropped{0};
        std::barrier<> ready (num_producers + 1 + 1);
        std::barrier<> go (num_producers + 1 + 1);

        std::vector<std::thread> producers;
        producers.reserve (num_producers);
        for (int p = 0; p < num_producers; ++p)
        {
            producers.emplace_back ([&, p] {
                pin_bench_thread (p);
                ready.arrive_and_wait ();
                go.arrive_and_wait ();
                std::uint64_t next = rdtsc ();
                for (std::size_t i = 0; i < items_per_producer; ++i)
                {
                    next += period;
                    while (rdtsc () < next)
                        cpu_pause ();
                    std::int64_t ts = static_cast<std::int64_t> (rdtsc ());
                    if (!q.push (ts))
                        dropped.fetch_add (1, std::memory_order_relaxed);
                }
            });
        }

        std::thread consumer ([&] {
            pin_bench_thread (num_producers);
            ready.arrive_and_wait ();
            go.arrive_and_wait ();
            std::int64_t ts;
            std::size_t count = 0;
            while (count + dropped.load (std::memory_order_relaxed) < total_items)
            {
                if (q.pop (ts))
                {
                    latencies.push_back (cycles_to_ns (rdtsc () - static_cast<std::uint64_t> (ts)));
                    ++count;
                }
                else
                {
                    cpu_pause ();
                }
            }
        });

        ready.arrive_and_wait ();
        go.arrive_and_wait ();

        for (auto& t : producers)
            t.join ();
        consumer.join ();

        last_dropped = dropped.load (std::memory_order_relaxed);
    }

    state.counters["smt"] = benchmark::Counter (nthreads > static_cast<int> (bench_cores ().size ()) ? 1.0 : 0.0);

    state.counters["dropped"] = benchmark::Counter (static_cast<double> (last_dropped));

    if (!latencies.empty ())
    {
        std::sort (latencies.begin (), latencies.end ());
        const auto n = static_cast<std::size_t> (latencies.size ());
        const double mean =
            static_cast<double> (std::accumulate (latencies.begin (), latencies.end (), std::int64_t{0})) / n;

        state.counters["lat_min_ns"] = benchmark::Counter (static_cast<double> (latencies.front ()));
        state.counters["lat_mean_ns"] = benchmark::Counter (mean);
        state.counters["lat_max_ns"] = benchmark::Counter (static_cast<double> (latencies.back ()));
        state.counters["lat_p50_ns"] = benchmark::Counter (static_cast<double> (latencies[n * 50 / 100]));
        state.counters["lat_p90_ns"] = benchmark::Counter (static_cast<double> (latencies[n * 90 / 100]));
        state.counters["lat_p99_ns"] = benchmark::Counter (static_cast<double> (latencies[n * 99 / 100]));
    }

    state.SetItemsProcessed (state.iterations () * static_cast<std::int64_t> (total_items));
    state.SetLabel (std::string (Adapter::name));
}

BENCHMARK (BM_MPSC_Latency<MoodycamelMPSC<std::int64_t>>)
    ->Name ("MPSC/Latency/Moodycamel")
    ->Args ({2, 4096})
    ->Args ({4, 4096})
    ->Args ({8, 4096})
    ->UseRealTime ()
    ->MinTime (0.5);

BENCHMARK (BM_MPSC_Latency<JoinMPSC<std::int64_t>>)
    ->Name ("MPSC/Latency/Join (Local)")
    ->Args ({2, 4096})
    ->Args ({4, 4096})
    ->Args ({8, 4096})
    ->UseRealTime ()
    ->MinTime (0.5);

BENCHMARK (BM_MPSC_Latency<JoinShmMPSC<std::int64_t>>)
    ->Name ("MPSC/Latency/Join (Shm)")
    ->Args ({2, 4096})
    ->Args ({4, 4096})
    ->Args ({8, 4096})
    ->UseRealTime ()
    ->MinTime (0.5);

BENCHMARK_MAIN ();
