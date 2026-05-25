#include <atomic>
#include <barrier>
#include <chrono>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/join_mpsc.hpp"
#include "adapters/moodycamel_mpsc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumItems = 1'000'000;

template<typename Adapter>
static void BM_MPSC_Throughput(benchmark::State& state)
{
    const int         num_producers     = static_cast<int>(state.range(0));
    const std::size_t capacity          = static_cast<std::size_t>(state.range(1));
    const std::size_t items_per_producer = kNumItems / static_cast<std::size_t>(num_producers);
    const std::size_t total_items        = items_per_producer * static_cast<std::size_t>(num_producers);

    for (auto _ : state) {
        Adapter q(capacity);
        std::barrier<> ready(num_producers + 1 + 1);
        std::barrier<> go   (num_producers + 1 + 1);

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

        std::thread consumer([&] {
            pin_bench_thread(num_producers);
            ready.arrive_and_wait();
            go.arrive_and_wait();
            int val;
            std::size_t count = 0;
            while (count < total_items) {
                if (q.pop(val)) ++count;
                else cpu_pause();
            }
        });

        ready.arrive_and_wait();
        go.arrive_and_wait();

        auto t0 = std::chrono::high_resolution_clock::now();
        for (auto& t : producers) t.join();
        consumer.join();
        auto t1 = std::chrono::high_resolution_clock::now();

        state.SetIterationTime(std::chrono::duration<double>(t1 - t0).count());
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kNumItems));
    state.SetLabel(std::string(Adapter::name));
}

BENCHMARK(BM_MPSC_Throughput<MoodycamelMPSC<int>>)
    ->Name("MPSC/Throughput/Moodycamel")
    ->Args({2, 4096})
    ->Args({4, 4096})
    ->Args({8, 4096})
    ->Args({2, 65536})
    ->Args({4, 65536})
    ->Args({8, 65536})
    ->UseManualTime();

BENCHMARK(BM_MPSC_Throughput<JoinMPSC<int>>)
    ->Name("MPSC/Throughput/Join")
    ->Args({2, 4096})
    ->Args({4, 4096})
    ->Args({8, 4096})
    ->Args({2, 65536})
    ->Args({4, 65536})
    ->Args({8, 65536})
    ->UseManualTime();

BENCHMARK_MAIN();
