#include <atomic>
#include <barrier>
#include <cstddef>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/join_bulk_mpmc.hpp"
#include "adapters/join_shm_bulk_mpmc.hpp"
#include "adapters/moodycamel_bulk_mpmc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumItems = 1'000'000;
static constexpr std::size_t kQueueCap = 4096;
static constexpr std::size_t kPublishChunk = 64;
static constexpr std::size_t kIdleCheck = 16;

template <typename Adapter>
static void BM_MPMC_Bulk (benchmark::State& state)
{
    const int num_producers = static_cast<int> (state.range (0));
    const int num_consumers = static_cast<int> (state.range (1));
    const std::size_t batch = static_cast<std::size_t> (state.range (2));
    const std::size_t items_per_producer = kNumItems / static_cast<std::size_t> (num_producers);
    const std::size_t total_items = items_per_producer * static_cast<std::size_t> (num_producers);
    const int total_threads = num_producers + num_consumers;
    if (total_threads > static_cast<int> (bench_cpus ().size ()))
    {
        state.SkipWithError ("not enough logical CPUs");
        return;
    }

    Adapter q (kQueueCap);

    for (auto _ : state)
    {
        std::atomic<std::size_t> total_consumed{0};
        std::barrier<> ready (total_threads + 1);
        std::barrier<> go (total_threads + 1);

        std::vector<std::thread> producers;
        producers.reserve (num_producers);
        for (int p = 0; p < num_producers; ++p)
        {
            producers.emplace_back ([&, p] {
                pin_bench_thread (p);
                std::vector<int> buf (batch, 42);
                ready.arrive_and_wait ();
                go.arrive_and_wait ();
                std::size_t remaining = items_per_producer;
                while (remaining > 0)
                {
                    std::size_t n = q.push_bulk (buf.data (), std::min (remaining, batch));
                    remaining -= n;
                    if (n == 0)
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
                std::vector<int> buf (batch);
                ready.arrive_and_wait ();
                go.arrive_and_wait ();
                std::size_t local = 0;
                std::size_t idle = 0;
                bool running = true;
                while (running)
                {
                    std::size_t n = q.pop_bulk (buf.data (), batch);
                    if (n > 0)
                    {
                        local += n;
                        if (local >= kPublishChunk)
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

    state.counters["smt"] = benchmark::Counter (total_threads > static_cast<int> (bench_cores ().size ()) ? 1.0 : 0.0);

    state.SetItemsProcessed (state.iterations () * static_cast<int64_t> (total_items));
    state.SetLabel (std::string (Adapter::name));
}

#define MPMC_BULK_ARGS       \
    ->Args ({1, 1, 8})       \
        ->Args ({1, 1, 64})  \
        ->Args ({1, 1, 256}) \
        ->Args ({2, 2, 64})  \
        ->Args ({2, 2, 256}) \
        ->Args ({4, 4, 64})  \
        ->Args ({4, 4, 256}) \
        ->UseManualTime ()

BENCHMARK (BM_MPMC_Bulk<JoinBulkMPMC<int>>)->Name ("MPMC/Bulk/Join (Local)") MPMC_BULK_ARGS;

BENCHMARK (BM_MPMC_Bulk<JoinShmBulkMPMC<int>>)->Name ("MPMC/Bulk/Join (Shm)") MPMC_BULK_ARGS;

BENCHMARK (BM_MPMC_Bulk<MoodycamelBulkMPMC<int>>)->Name ("MPMC/Bulk/Moodycamel") MPMC_BULK_ARGS;

BENCHMARK_MAIN ();
