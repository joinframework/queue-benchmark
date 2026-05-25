# queue-benchmark

Comparative benchmarks of lock-free queues in C++20: throughput, latency, and bulk transfer for SPSC, MPSC, and MPMC patterns.

## Libraries

| Library | SPSC | MPSC | MPMC | Bulk |
|---------|------|------|------|------|
| [Boost.Lockfree](https://www.boost.org/doc/libs/release/libs/lockfree/) | ✓ | — | ✓ | — |
| [Moodycamel](https://github.com/cameron314/concurrentqueue) | ✓ | ✓ | ✓ | MPSC / MPMC |
| [Rigtorp SPSCQueue](https://github.com/rigtorp/SPSCQueue) | ✓ | — | — | — |
| [Rigtorp MPMCQueue](https://github.com/rigtorp/MPMCQueue) | — | — | ✓ | — |
| [join](https://github.com/joinframework/join) | ✓ | ✓ | ✓ | ✓ |
| [Folly](https://github.com/facebook/folly) *(optional)* | — | — | ✓ | — |

All dependencies except Folly are fetched automatically via CMake FetchContent.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Optional: Folly MPMC benchmarks

Install Folly system-wide, then re-run CMake — Folly benchmarks are enabled automatically:

```bash
sudo apt install libfolly-dev   # or build from source
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

CMake prints `Folly found — Folly MPMC benchmarks enabled` when detected.

## Running benchmarks

```bash
# Run everything and generate plots
cd scripts && bash run_bench.sh

# Single binary
./build/benchmarks/spsc/bench_spsc_throughput \
    --benchmark_format=json \
    --benchmark_out=results/spsc_throughput.json

# Filter a specific configuration
./build/benchmarks/mpmc/bench_mpmc_throughput --benchmark_filter="4/4/"
```

Default: 3 repetitions. Override with `REPETITIONS=5 bash run_bench.sh`.

## Plots

```bash
pip install -r plots/requirements.txt
cd plots && python3 plot.py ../results/*.json
```

PNGs are written to `plots/`.

---

## What the benchmarks measure

### Throughput

**Goal**: maximum sustained transfer rate (items/s) under full load.

**Protocol**:
- Two `std::barrier` instances synchronize startup: all threads wait at `ready`, then are released simultaneously by `go`. This eliminates thread creation overhead from the measurement.
- Producers push `N` items as fast as possible (`while (!push) cpu_pause()`).
- Consumers drain until an atomic counter reaches `N`.
- Wall time is measured between `go` and the last thread join (`UseManualTime`).

**What is measured**: saturated pipe throughput — how fast the queue can transfer items when producers and consumers run flat-out simultaneously.

**What is not measured**: per-item latency, startup effects, thread creation costs.

**SPSC parameters**: capacity = 256 / 4096 / 65536.  
**MPSC/MPMC parameters**: {nProducers, nConsumers, capacity}, with symmetric configs (1P/1C, 2P/2C, 4P/4C, 8P/8C) and asymmetric ones (4P/1C, 1P/4C).

---

### Bulk throughput

**Goal**: throughput when transfers are batched — amortizing the cost of atomic operations over multiple items per call.

**Protocol**: identical to throughput, but producers call `push_bulk(buf, batch)` and consumers call `pop_bulk(buf, batch)` instead of single-item operations. Batch sizes tested: 8, 64, 256.

**What is measured**: how much throughput gain (or loss) a library's native bulk API provides compared to single-item transfers. Only libraries with a native bulk API are included — simulating bulk with a loop would measure loop overhead, not bulk performance.

**SPSC parameters**: {batch_size}.  
**MPSC parameters**: {nProducers, batch_size}.  
**MPMC parameters**: {nProducers, nConsumers, batch_size}.

---

### SPSC latency (ping-pong)

**Goal**: round-trip time of a single item between two threads.

**Protocol**:
```
main ──[push(42)]──▶ request_q ──▶ consumer ──[push(42)]──▶ reply_q ──▶ main
```
- The consumer thread runs continuously for the entire benchmark duration.
- For each round-trip: `t0 = rdtsc()` → push to `request_q` → spin-wait on `reply_q` → `delta = rdtsc() - t0`.
- 100,000 round-trips per Google Benchmark iteration. Samples are sorted at the end to compute mean / P50 / P99.
- Timing uses the TSC (Time Stamp Counter) calibrated at program startup: ~5 cycles overhead vs ~25 ns for `clock_gettime`.

**What is measured**: round-trip latency — total time from the intent to push until the reply is received. Includes two queue traversals and two cross-core cache coherency transfers. Only one item is in flight at a time, so there is no queuing delay.

**What is not measured**: one-way latency (divide by 2 for an estimate).

---

### MPSC / MPMC latency (embedded timestamp)

**Goal**: per-item latency from push to pop under realistic concurrent load.

**Protocol**:
- Each producer embeds a TSC timestamp into the item at push time: `item = rdtsc()`.
- Each consumer computes the delta at pop time: `latency = rdtsc() - item`.
- 100,000 items total per iteration. Latencies are collected per consumer and merged at the end to compute mean / P50 / P99.

**What is measured**: total time an item spends in the system, from the intent to push until it is popped. This includes:
1. Producer spin time waiting for a free slot if the queue is full (the timestamp is captured *before* the push loop).
2. Time waiting in the queue.
3. Time until the consumer reaches this item.

**Interpretation**: producers and consumers run simultaneously at full speed, so the queue tends to stay saturated. The measured latency is therefore dominated by **queuing delay** (Little's Law: L = λ × W), not by the queue's intrinsic service time. A high-throughput queue running a saturated ring will mechanically produce higher measured latency. This benchmark is most useful for detecting pathologically high individual latency — such as Moodycamel MPSC, whose batch-oriented design produces per-item latencies in the millisecond range.

---

## Thread synchronization pattern

All benchmarks use the same two-barrier startup pattern:

```
threads          main
  ┌─ ready ◀────────────────────────────── arrive_and_wait()
  │  ready ────────────────────────────────────────────────┐
  └─ go    ◀────────────────────────────── arrive_and_wait()
           ────────────────────────────────────────────────┘
           [measurement starts here]
```

The `ready` barrier ensures all threads are created and initialized. The `go` barrier releases them simultaneously to eliminate staggered-start effects.

---

## Notes

- `-march=native`: CPU-specific optimizations. Results are not portable across machines.
- TSC is calibrated over 20 ms at program startup, before any benchmark runs.
- `REPETITIONS=3` by default: Google Benchmark reports the mean across repetitions.
- Boost.Lockfree MPMC uses `fixed_sized<true>`: fixed ring buffer with no dynamic allocation, guaranteeing lock-freedom.
- Result JSONs and plots are gitignored (`results/`, `plots/*.png`).
