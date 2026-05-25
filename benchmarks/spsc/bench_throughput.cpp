#include <barrier>
#include <chrono>
#include <thread>

#include <benchmark/benchmark.h>

#include "adapters/boost_spsc.hpp"
#include "adapters/join_spsc.hpp"
#include "adapters/moodycamel_spsc.hpp"
#include "adapters/rigtorp_spsc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumItems = 1'000'000;

template<typename Adapter>
static void BM_SPSC_Throughput(benchmark::State& state)
{
    const std::size_t capacity = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        Adapter q(capacity);
        std::barrier<> ready(3);
        std::barrier<> go(3);

        std::thread producer([&] {
            pin_bench_thread(0);
            ready.arrive_and_wait();
            go.arrive_and_wait();
            for (std::size_t i = 0; i < kNumItems; ++i)
                while (!q.push(static_cast<int>(i))) cpu_pause();
        });

        std::thread consumer([&] {
            pin_bench_thread(1);
            ready.arrive_and_wait();
            go.arrive_and_wait();
            int val;
            std::size_t count = 0;
            while (count < kNumItems) {
                if (q.pop(val)) ++count;
                else cpu_pause();
            }
        });

        ready.arrive_and_wait();
        go.arrive_and_wait();

        auto t0 = std::chrono::high_resolution_clock::now();
        producer.join();
        consumer.join();
        auto t1 = std::chrono::high_resolution_clock::now();

        state.SetIterationTime(std::chrono::duration<double>(t1 - t0).count());
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kNumItems));
    state.SetLabel(std::string(Adapter::name));
}

// capacity : 256 / 4096 / 65536
BENCHMARK(BM_SPSC_Throughput<BoostSPSC<int>>)
    ->Name("SPSC/Throughput/Boost")
    ->Arg(256)->Arg(4096)->Arg(65536)
    ->UseManualTime();

BENCHMARK(BM_SPSC_Throughput<MoodycamelSPSC<int>>)
    ->Name("SPSC/Throughput/Moodycamel")
    ->Arg(256)->Arg(4096)->Arg(65536)
    ->UseManualTime();

BENCHMARK(BM_SPSC_Throughput<RigtorpSPSC<int>>)
    ->Name("SPSC/Throughput/Rigtorp")
    ->Arg(256)->Arg(4096)->Arg(65536)
    ->UseManualTime();

BENCHMARK(BM_SPSC_Throughput<JoinSPSC<int>>)
    ->Name("SPSC/Throughput/Join")
    ->Arg(256)->Arg(4096)->Arg(65536)
    ->UseManualTime();

BENCHMARK_MAIN();
