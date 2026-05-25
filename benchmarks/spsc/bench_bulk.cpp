#include <barrier>
#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/join_bulk_spsc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumItems   = 1'000'000;
static constexpr std::size_t kQueueCap   = 4096;

template<typename Adapter>
static void BM_SPSC_Bulk(benchmark::State& state)
{
    const std::size_t batch = static_cast<std::size_t>(state.range(0));

    std::vector<int> push_buf(batch, 42);
    std::vector<int> pop_buf(batch);

    for (auto _ : state) {
        Adapter q(kQueueCap);
        std::barrier<> ready(3);
        std::barrier<> go(3);

        std::thread producer([&] {
            pin_bench_thread(0);
            ready.arrive_and_wait();
            go.arrive_and_wait();
            std::size_t remaining = kNumItems;
            while (remaining > 0) {
                std::size_t n = q.push_bulk(push_buf.data(),
                                            std::min(remaining, batch));
                remaining -= n;
                if (n == 0) cpu_pause();
            }
        });

        std::thread consumer([&] {
            pin_bench_thread(1);
            ready.arrive_and_wait();
            go.arrive_and_wait();
            std::size_t consumed = 0;
            while (consumed < kNumItems) {
                std::size_t n = q.pop_bulk(pop_buf.data(), batch);
                consumed += n;
                if (n == 0) cpu_pause();
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

BENCHMARK(BM_SPSC_Bulk<JoinBulkSPSC<int>>)
    ->Name("SPSC/Bulk/Join")
    ->Arg(8)->Arg(64)->Arg(256)
    ->UseManualTime();

BENCHMARK_MAIN();
