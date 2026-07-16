#include <algorithm>
#include <cstdint>
#include <numeric>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "adapters/boost_spsc.hpp"
#include "adapters/join_spsc.hpp"
#include "adapters/join_shm_spsc.hpp"
#include "adapters/moodycamel_spsc.hpp"
#include "adapters/rigtorp_spsc.hpp"
#include "common/utils.hpp"

static constexpr std::size_t kNumLatencyItems = 100'000;

template <typename Adapter>
static void BM_SPSC_Latency (benchmark::State& state)
{
    Adapter request_q (64);
    Adapter reply_q (64);

    std::vector<std::int64_t> samples;
    samples.reserve (kNumLatencyItems);

    pin_bench_thread (0);

    std::thread consumer ([&] {
        pin_bench_thread (1);
        int val;
        while (true)
        {
            if (request_q.pop (val))
            {
                if (val < 0)
                    break;
                while (!reply_q.push (val))
                    cpu_pause ();
            }
            else
            {
                cpu_pause ();
            }
        }
    });

    for (auto _ : state)
    {
        samples.clear ();
        for (std::size_t i = 0; i < kNumLatencyItems; ++i)
        {
            std::uint64_t t0 = rdtsc ();
            while (!request_q.push (42))
                cpu_pause ();
            int val;
            while (!reply_q.pop (val))
                cpu_pause ();
            samples.push_back (cycles_to_ns (rdtsc () - t0));
        }
    }

    while (!request_q.push (-1))
        cpu_pause ();
    consumer.join ();

    if (!samples.empty ())
    {
        std::sort (samples.begin (), samples.end ());
        const auto n = samples.size ();
        const double mean =
            static_cast<double> (std::accumulate (samples.begin (), samples.end (), std::int64_t{0})) / n;

        state.counters["lat_min_ns"] = benchmark::Counter (static_cast<double> (samples.front ()));
        state.counters["lat_mean_ns"] = benchmark::Counter (mean);
        state.counters["lat_max_ns"] = benchmark::Counter (static_cast<double> (samples.back ()));
        state.counters["lat_p50_ns"] = benchmark::Counter (static_cast<double> (samples[n * 50 / 100]));
        state.counters["lat_p90_ns"] = benchmark::Counter (static_cast<double> (samples[n * 90 / 100]));
        state.counters["lat_p99_ns"] = benchmark::Counter (static_cast<double> (samples[n * 99 / 100]));
    }

    state.SetItemsProcessed (state.iterations () * static_cast<std::int64_t> (kNumLatencyItems));
    state.SetLabel (std::string (Adapter::name));
}

BENCHMARK (BM_SPSC_Latency<BoostSPSC<int>>)->Name ("SPSC/Latency/Boost")->UseRealTime ()->MinTime (0.5);

BENCHMARK (BM_SPSC_Latency<MoodycamelSPSC<int>>)->Name ("SPSC/Latency/Moodycamel")->UseRealTime ()->MinTime (0.5);

BENCHMARK (BM_SPSC_Latency<RigtorpSPSC<int>>)->Name ("SPSC/Latency/Rigtorp")->UseRealTime ()->MinTime (0.5);

BENCHMARK (BM_SPSC_Latency<JoinSPSC<int>>)->Name ("SPSC/Latency/Join (Local)")->UseRealTime ()->MinTime (0.5);

BENCHMARK (BM_SPSC_Latency<JoinShmSPSC<int>>)->Name ("SPSC/Latency/Join (Shm)")->UseRealTime ()->MinTime (0.5);

BENCHMARK_MAIN ();
