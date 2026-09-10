#include <atomic>
#include <barrier>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/boost_mpmc.hpp"
#include "adapters/join_mpmc.hpp"
#include "adapters/join_shm_mpmc.hpp"
#include "adapters/moodycamel_mpmc.hpp"
#include "adapters/rigtorp_mpmc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumItems = 1'000'000;

static constexpr std::size_t kPublishChunk = 64;
static constexpr std::size_t kIdleCheck = 16;

template <typename Adapter>
static void BM_MPMC_Throughput (benchmark::State& state)
{
    const int num_producers = static_cast<int> (state.range (0));
    const int num_consumers = static_cast<int> (state.range (1));
    const int nthreads = num_producers + num_consumers;
    if (nthreads > static_cast<int> (bench_cpus ().size ()))
    {
        state.SkipWithError ("not enough logical CPUs");
        return;
    }

    const std::size_t capacity = static_cast<std::size_t> (state.range (2));
    const std::size_t items_per_producer = kNumItems / static_cast<std::size_t> (num_producers);
    const std::size_t total_items = items_per_producer * static_cast<std::size_t> (num_producers);

    Adapter q (capacity);

    for (auto _ : state)
    {
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
                const std::size_t start = static_cast<std::size_t> (p) * items_per_producer;
                for (std::size_t i = start; i < start + items_per_producer; ++i)
                    while (!q.push (static_cast<int> (i)))
                        cpu_pause ();
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
                int val;
                std::size_t local = 0;
                std::size_t idle = 0;
                bool running = true;
                while (running)
                {
                    if (q.pop (val))
                    {
                        if (++local == kPublishChunk)
                        {
                            running = total_consumed.fetch_add (local, std::memory_order_relaxed) + local < total_items;
                            local = 0;
                        }
                    }
                    else
                    {
                        if (++idle == kIdleCheck)
                        {
                            if (local != 0)
                            {
                                total_consumed.fetch_add (local, std::memory_order_relaxed);
                                local = 0;
                            }
                            running = total_consumed.load (std::memory_order_relaxed) < total_items;
                            idle = 0;
                        }
                        cpu_pause ();
                    }
                }
            });
        }

        ready.arrive_and_wait ();
        go.arrive_and_wait ();

        auto t0 = rdtsc ();
        for (auto& t : producers)
            t.join ();
        for (auto& t : consumers)
            t.join ();
        state.SetIterationTime (static_cast<double> (cycles_to_ns (rdtsc () - t0)) * 1e-9);
    }

    state.counters["smt"] = benchmark::Counter (nthreads > static_cast<int> (bench_cores ().size ()) ? 1.0 : 0.0);
    state.SetItemsProcessed (state.iterations () * static_cast<int64_t> (kNumItems));
    state.SetLabel (std::string (Adapter::name));
}

#define MPMC_ARGS             \
    ->Args ({1, 1, 4096})     \
        ->Args ({2, 2, 4096}) \
        ->Args ({4, 4, 4096}) \
        ->Args ({8, 8, 4096}) \
        ->Args ({4, 1, 4096}) \
        ->Args ({1, 4, 4096}) \
        ->UseManualTime ()

BENCHMARK (BM_MPMC_Throughput<BoostMPMC<int>>)->Name ("MPMC/Throughput/Boost") MPMC_ARGS;

BENCHMARK (BM_MPMC_Throughput<MoodycamelMPMC<int>>)->Name ("MPMC/Throughput/Moodycamel") MPMC_ARGS;

BENCHMARK (BM_MPMC_Throughput<JoinMPMC<int>>)->Name ("MPMC/Throughput/Join (Local)") MPMC_ARGS;

BENCHMARK (BM_MPMC_Throughput<JoinShmMPMC<int>>)->Name ("MPMC/Throughput/Join (Shm)") MPMC_ARGS;

BENCHMARK (BM_MPMC_Throughput<RigtorpMPMC<int>>)->Name ("MPMC/Throughput/Rigtorp") MPMC_ARGS;

BENCHMARK_MAIN ();
