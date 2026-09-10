# queue-benchmark

Comparative benchmarks of lock-free queue libraries in C++, measuring throughput, latency, and bulk transfer across single and multi producer/consumer patterns (SPSC, MPSC, MPMC). Includes ready-to-use plots and a breakdown of what each benchmark actually measures.

## Table of contents

- [Libraries](#libraries)
- [Build](#build)
- [Running benchmarks](#running-benchmarks)
- [Plots](#plots)
- [What the benchmarks measure](#what-the-benchmarks-measure)
  - [Throughput](#throughput)
  - [Bulk throughput](#bulk-throughput)
  - [SPSC latency (ping-pong)](#spsc-latency-ping-pong)
  - [MPSC / MPMC latency (paced)](#mpsc--mpmc-latency-paced)
- [Thread synchronization pattern](#thread-synchronization-pattern)
- [Notes](#notes)
- [Test machine](#test-machine)
- [Results](#results)
  - [SPSC](#spsc)
  - [MPSC](#mpsc)
  - [MPMC](#mpmc)
  - [Conclusion](#conclusion)
- [License](#license)

## Libraries

| Library | SPSC | MPSC | MPMC | Bulk |
|---------|------|------|------|------|
| [Boost](https://www.boost.org/doc/libs/release/libs/lockfree/) | ✓ | — | ✓ | — |
| [Moodycamel](https://github.com/cameron314/concurrentqueue) | ✓ | ✓ | ✓ | MPSC / MPMC |
| [Rigtorp](https://github.com/rigtorp/SPSCQueue) | ✓ | — | ✓ | — |
| [Join](https://github.com/joinframework/join) (LocalMem) | ✓ | ✓ | ✓ | ✓ |
| [Join](https://github.com/joinframework/join) (ShmMem) | ✓ | ✓ | ✓ | ✓ |

All dependencies are fetched automatically via CMake FetchContent.

**Join** is the only library benchmarked here that can back its ring buffer with POSIX shared memory (`ShmMem`, via `shm_open`/`mmap(MAP_SHARED)`) instead of process-local anonymous memory (`LocalMem`, `mmap(MAP_ANONYMOUS)`). Both variants run in the same process in this benchmark, **Join (Shm)** measures the overhead of the shared-memory backend itself (POSIX shm bookkeeping vs. anonymous mmap), not real inter-process transfer, since actual cross-process IPC would require a separate producer/consumer process pair.

## Build

Configure the project:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Compile:

```bash
cmake --build build -j$(nproc)
```

## Running benchmarks

Run everything and generate plots:

```bash
bash scripts/run_bench.sh
```

> [!IMPORTANT]
> Run it from the repository root. The script configures `cmake -B build` relative to the current directory, so invoking it from inside `scripts/` creates a build tree in the wrong place.

Single binary:

```bash
./build/benchmarks/spsc/bench_spsc_throughput \
    --benchmark_format=json \
    --benchmark_out=results/spsc_throughput.json
```

Filter a specific configuration, `--benchmark_filter` takes a regex matched against the benchmark name (e.g. `MPMC/Throughput/Join (Local)/4/4/4096`); this one keeps only the 4-producer/4-consumer runs:

```bash
./build/benchmarks/mpmc/bench_mpmc_throughput --benchmark_filter="4/4/"
```

> [!NOTE]
> Default: 10 repetitions. Override with `REPETITIONS=5 bash run_bench.sh`.

## Plots

Install plotting dependencies:

```bash
pip install -r plots/requirements.txt
```

Generate the charts:

```bash
cd plots && python3 plot.py ../results/*.json
```

PNGs are written to `plots/`. Latency results produce two charts per queue type: `*_latency_dist.png` (Min/Mean/Max) and `*_latency_pct.png` (P50/P90/P99).

---

## What the benchmarks measure

### Throughput

**Goal**: maximum sustained transfer rate (items/s) under full load.

**Protocol**:
- Two `std::barrier` instances synchronize startup: all threads wait at `ready`, then are released simultaneously by `go`. This eliminates thread creation overhead from the measurement.
- Producers push `N` items as fast as possible (`while (!push) cpu_pause()`).
- Consumers drain until an atomic counter reaches `N`.
- Wall time is measured between `go` and the last thread join using the TSC (`UseManualTime`).

**What is measured**: saturated pipe throughput, how fast the queue can transfer items when producers and consumers run flat-out simultaneously.

**What is not measured**: per-item latency, startup effects, thread creation costs.

**SPSC parameters**: capacity = 256 / 4096 / 65536.  
**MPSC/MPMC parameters**: {nProducers, nConsumers, capacity}, with symmetric configs (1P/1C, 2P/2C, 4P/4C, 8P/8C) and asymmetric ones (4P/1C, 1P/4C).

---

### Bulk throughput

**Goal**: throughput when transfers are batched, amortizing the cost of atomic operations over multiple items per call.

**Protocol**: identical to throughput, but producers call `push_bulk(buf, batch)` and consumers call `pop_bulk(buf, batch)` instead of single-item operations. Batch sizes tested: 8, 64, 256.

**What is measured**: how much throughput gain (or loss) a library's native bulk API provides compared to single-item transfers. Only libraries with a native bulk API are included, simulating bulk with a loop would measure loop overhead, not bulk performance.

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
- 100,000 round-trips per Google Benchmark iteration. Samples are sorted at the end to compute min / mean / max / P50 / P90 / P99.
- Timing uses the TSC (Time Stamp Counter) calibrated at program startup: ~5 cycles overhead vs ~25 ns for `clock_gettime`.

**What is measured**: round-trip latency, total time from the intent to push until the reply is received. Includes two queue traversals and two cross-core cache coherency transfers. Only one item is in flight at a time, so there is no queuing delay.

**What is not measured**: one-way latency (divide by 2 for an estimate).

---

### MPSC / MPMC latency (paced)

**Goal**: per-item transit latency from push to pop, at an offered load held below saturation.

**Protocol**:
- Producers are **rate-paced**: each spins on the TSC until its next slot, so the aggregate offered load is a fixed 1M items/s whatever the producer count.
- Each producer embeds a TSC timestamp in the item, taken after the pacing wait and immediately before the push: `item = rdtsc()`.
- A failed push is **not retried**. The item increments a `dropped` counter and the producer moves on to its next slot, so producer backpressure never enters the measurement.
- Each consumer computes `latency = rdtsc() - item` at pop time.
- 50,000 items per iteration, merged across consumers to compute min / mean / max / P50 / P90 / P99.

**What is measured**: the transit time of an item through the queue, one way, under concurrent access but without queuing delay.

**Reading the result**: `dropped` must be 0. A non-zero value means the offered load exceeded what the queue could absorb, the ring filled up, and the latency figures are contaminated by queuing delay. They are then to be discarded, not interpreted.

**Why the pacing is not optional**: with producers running flat out, the ring saturates permanently and the pre-push timestamp charges each item with the producer's spin time on a full queue. What comes out is Little's Law, `capacity / throughput`, and it carries no information the throughput benchmark does not already give. An earlier version of this benchmark did exactly that: its MPMC 1P/1C figure for **Join** was 249 µs, against 189 ns for the same code measured by the SPSC ping-pong. Cross-checking the two JSON files reproduced `4096 / throughput` to within 0.2%.

**Thread placement is explicit.** Threads are pinned in a fixed order: one per physical core first, then the SMT siblings. A configuration needing more threads than there are physical cores is still measured — it is often the interesting one — but reports `smt=1` so the reader knows some pairs share a core. Only a configuration exceeding the logical CPU count is skipped, with `not enough logical CPUs`.

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
- `REPETITIONS=10` by default: Google Benchmark reports the mean across repetitions. SPSC latency is sensitive to OS scheduling noise on its single ping-pong thread pair, so more repetitions help reduce run-to-run variance.
- All benchmarks time with the TSC exclusively (throughput, bulk, and latency alike) for consistent, low-overhead measurement.
- `run_bench.sh` sets `--benchmark_min_warmup_time=0.1` to stabilize CPU frequency scaling before measurement starts.
- Boost MPMC uses `fixed_sized<true>`: fixed ring buffer with no dynamic allocation, guaranteeing lock-freedom.
- Every adapter is **bounded by its declared capacity**. Moodycamel's bulk adapters use `try_enqueue_bulk`, not `enqueue_bulk`: the latter grows the queue by allocating, so it never fails and its capacity argument means nothing, which would compare it against the other libraries under different rules.
- Threads are pinned in a deterministic order derived from `/sys/devices/system/cpu/*/topology/thread_siblings_list`: one per physical core first, then the SMT siblings. This matters: two threads sharing a physical core communicate through a shared L1 and skip cross-core coherency traffic entirely, which inflates SPSC throughput several-fold and produces figures that describe the placement rather than the queue. Leaving threads unpinned is worse still, since the placement then changes from run to run. Benchmarks report `smt=1` when the configuration is dense enough to use sibling threads.
- Consumers count what they pop in a **local** counter and publish to the shared total once per 64 items, or after 16 consecutive failed pops. Doing the atomic increment per item instead puts a contended cache line in the tightest loop of the benchmark and measures the harness rather than the queue: it cost up to 5x on MPMC throughput, and — worse — it penalised the libraries unequally, which reversed a published ranking.
- Build and measurement are separated. A `-j$(nproc)` compile heats the package enough that the first benchmark afterwards runs below the rated clock; see [Test machine](#test-machine).
- These benchmarks are meaningless under a hypervisor that does not expose host topology, WSL2 included: `pthread_setaffinity_np` then pins to a virtual CPU that the hypervisor remains free to schedule on any physical core, so core placement cannot be controlled from inside the guest.
- Result JSONs are gitignored (`results/`); plots are committed under `plots/`.

---

## Test machine

All figures below come from a single run on this machine. They are not portable: `-march=native` alone makes them specific to this CPU.

| | |
|---|---|
| CPU | AMD Ryzen 7 2700X, 8 cores / 16 threads (Zen+, `znver1`), 1 socket, 1 NUMA node |
| Frequency | 3700 MHz, `performance` governor, boost disabled |
| Caches | L1d 8 × 32 KiB, L1i 8 × 64 KiB, L2 8 × 512 KiB, L3 2 × 8 MiB |
| SMT pairing | logical CPU `n` and `n+8` share one physical core |
| TSC | `constant_tsc` + `nonstop_tsc` (invariant), calibrated over 20 ms at startup |
| RAM | 16 GiB |
| OS | Ubuntu 24.04.4 LTS, kernel 6.8.0-138-generic |
| Toolchain | GCC 13.3.0, glibc 2.39, CMake 3.28.3 |
| Compile flags | `-O3 -DNDEBUG -march=native -std=c++20` |
| THP | `madvise`, explicit hugepage pool empty (`HugePages_Total: 0`) |
| Conditions | idle machine, 10 repetitions, run started at 3692 MHz measured |

### Running conditions matter more than they look

Two effects had to be controlled before any of these numbers were reproducible.

**Compile heat.** `run_bench.sh` builds with `-j$(nproc)` and starts measuring immediately afterwards. On this CPU a 16-core compile pushes the package out of its power envelope, and the first benchmark then runs on a chip still recovering: one run started at 3178 MHz instead of 3700 and produced coefficients of variation of 13% to 32% on benchmarks whose code had not changed. Build first, let the machine settle, measure second.

**Frequency, not load.** Waiting for the load average or for the top process to go quiet is not enough, because the chip can be idle and still below its rated clock. The usable check is the **maximum** per-core frequency in `/proc/cpuinfo`: the average across all 16 CPUs is dominated by halted cores and reads ~3600 even on a perfectly idle machine, so it makes a poor gate.

One limitation is inherent and not fixed here: within a single run the light families are measured first on a cool chip and the heavy ones last on a hot one. Comparisons **between libraries** stay valid, since they are measured side by side within a family; comparisons **between families** are looser.

`cv` in the tables below is the coefficient of variation across the 10 repetitions. It is the noise floor of each measurement: a gap smaller than the `cv` of the two figures being compared is not a result.

---

## Results

### SPSC

![SPSC Throughput](plots/spsc_throughput.png)

Throughput, M items/s. **This table is the median of four independent runs** of 10 repetitions each, with the range across those runs and the ratio max/min:

| capacity | Join (Local) | Join (Shm) | Rigtorp | Moodycamel | Boost |
|---|---|---|---|---|---|
| 256 | **175.1** (170–185, ×1.08) | 170.6 (167–177, ×1.06) | 91.4 (85–97, ×1.14) | 152.8 (131–157, ×1.20) | 117.5 (107–131, ×1.23) |
| 4096 | **175.4** (173–184, ×1.06) | 167.0 (156–177, ×1.14) | 88.1 (83–120, ×1.44) | *bimodal*, 126–294 | 127.4 (121–133, ×1.10) |
| 65536 | 130.5 (104–178, ×1.71) | 132.4 (115–197, ×1.71) | 115.7 (69–140, ×2.02) | 126.9 (107–168, ×1.57) | 99.8 (61–140, ×2.29) |

> [!WARNING]
> The chart above plots one value per bar and therefore cannot represent the **Moodycamel** 4096 case, which is a mixture of two modes rather than a measurement. Read that bar from the table, not from the plot.

Only the first row supports a ranking, and there **Join** wins outright: its range, 170 to 185, is disjoint from **Moodycamel**'s 131 to 157, the next best.

At capacity 4096 **Moodycamel** has no single throughput figure, because its distribution is **bimodal**. Over 20 repetitions in one process, 17 landed between 125.7 and 131.2 M items/s and 3 between 163.2 and 179.0; across four other processes the run means were 164, 196, 242 and 294 M items/s, meaning the fast mode dominated there instead. **Join** measured in the same processes stays unimodal around 175. No point estimate is honest here, so the table gives the observed span, 126 to 294.

The likely mechanism is the classic SPSC convoy effect: depending on the gap that establishes itself at startup, producer and consumer either work on the same cache line — every item then costs a coherency transfer, the slow mode — or the consumer trails by at least a line, letting the producer fill a whole line before it is read, the fast mode. The regime is set at launch and persists. This is a real property of the queue under this workload, not a benchmark defect, but it does mean a single number cannot describe it.

At capacity 65536 every library disperses by 1.6x to 2.3x and no ranking survives. The ring is 256 KiB there, well past L1 and deep into L2, so conflict misses dominate and page placement decides.

**Why four runs, and why per-repetition values.** `--benchmark_repetitions` repeats inside a single process, on a queue allocated once, hence at one fixed physical page placement and one fixed mapping into cache sets. Every repetition inherits that layout, so the reported `cv` measures only what varies *within* a process, and it is a lower bound: at capacity 65536 it reads 0.6% to 3% while the spread across processes reaches 2.3x. Averages hide the rest — the **Moodycamel** 4096 mixture is invisible in any mean or `cv`, and only shows up when the individual repetitions are listed.

> [!NOTE]
> This multi-run treatment was applied to SPSC throughput only. Every other table in this document comes from a single process, so its `cv` is a lower bound on the real uncertainty. Configurations with more threads should be less layout-sensitive, since contention dominates, but that has not been measured here.

![SPSC Latency Distribution](plots/spsc_latency_dist.png)
![SPSC Latency Percentiles](plots/spsc_latency_pct.png)

Round-trip latency, ns:

| | P50 | P90 | P99 | max |
|---|---|---|---|---|
| Join (Shm) | **179** | 199 | 222 | 3 866 |
| Boost | **179** | 209 | 210 | 4 122 |
| Join (Local) | 183 | 196 | 216 | 5 178 |
| Rigtorp | 200 | 201 | 212 | 4 887 |
| Moodycamel | 210 | 220 | 229 | 4 104 |

The five sit within 17% of each other, and the top three within the run-to-run spread. A round-trip is two cache-line transfers between physical cores; that cost belongs to the hardware, not to the library, and no ranking should be read into it. The maxima (3.9–5.2 µs) are scheduler preemptions.

![SPSC Bulk Throughput](plots/spsc_bulk.png)

Bulk throughput, M items/s. Only **Join** exposes a native SPSC bulk API:

| batch | Join (Local) | Join (Shm) |
|---|---|---|
| 8 | 513.6 (cv 21.2%) | 561.0 (cv 10.0%) |
| 64 | **1 897.6** | 1 824.1 |
| 256 | **2 838.7** | 2 788.3 |

Batching is the single largest effect in this whole benchmark: 2.84 G items/s at batch 256, **16.2x** the single-item rate, because a bulk transfer is two `memcpy` and one index publication instead of one publication per item. The batch-8 row is too noisy to rank the two backends.

### MPSC

![MPSC Throughput](plots/mpsc_throughput.png)

Throughput, M items/s:

| producers / capacity | Join (Local) | Join (Shm) | Moodycamel |
|---|---|---|---|
| 2 / 4096 | 27.1 | **27.2** | 8.3 |
| 4 / 4096 | 20.6 | **20.7** | 5.4 |
| 8 / 4096 | 15.6 | **15.7** | 6.0 |
| 2 / 65536 | **26.3** | 26.2 | 11.2 |
| 4 / 65536 | **19.7** | 19.2 | 5.3 |
| 8 / 65536 | 16.8 | **17.6** | 8.1 |

**Join** is 2.2x to 3.8x **Moodycamel** in every configuration, with `cv` at or below 4%. The two backends are within 3% of each other throughout. Throughput falls as producers are added (27.1 → 15.6 M items/s from 2P to 8P): the CAS on the head index is the bottleneck, and more producers means more failed attempts.

![MPSC Latency Distribution](plots/mpsc_latency_dist.png)
![MPSC Latency Percentiles](plots/mpsc_latency_pct.png)

Latency at 1M items/s offered, ns, `dropped` = 0 everywhere:

| producers | | P50 | P90 | P99 | max |
|---|---|---|---|---|---|
| 2 | Join (Local) | **89** | 90 | 98 | 11 096 |
| 2 | Join (Shm) | 90 | 102 | 110 | 11 409 |
| 2 | Moodycamel | 132 | 164 | 317 | 10 828 |
| 4 | Join (Local) | **224** | 259 | 273 | 12 446 |
| 4 | Join (Shm) | 246 | 290 | 497 | 12 658 |
| 4 | Moodycamel | 410 | 522 | 1 003 | 12 421 |
| 8 | Join (Shm) | **201** | 312 | 496 | 16 580 |
| 8 | Join (Local) | 209 | 322 | 460 | 12 950 |
| 8 | Moodycamel | 356 | 552 | 1 130 | 15 109 |

**Join** transits an item in 89 ns with 2 producers and holds a 1.5x to 1.8x lead over **Moodycamel** at every producer count, with a P99 three to four times tighter.

The 8-producer row carries `smt=1`: 9 threads on 8 physical cores, so one pair shares a core. Its P50 is *lower* than the 4-producer row, which is expected rather than surprising — the offered rate is fixed at 1M items/s in total, so more producers means each one sends less often and the arrivals are less bursty.

![MPSC Bulk Throughput](plots/mpsc_bulk.png)

Bulk throughput, M items/s:

| producers / batch | Join (Local) | Join (Shm) | Moodycamel |
|---|---|---|---|
| 2 / 8 | **93.5** | 66.0 | 54.6 |
| 2 / 64 | **482.2** | 368.2 | 183.5 |
| 2 / 256 | **665.7** | 604.6 | 174.5 |
| 4 / 64 | **133.1** | 110.6 | 67.2 |
| 4 / 256 | **169.7** | 155.8 | 89.8 |

**Join (Local)** leads every configuration, from 1.7x to 3.8x. Unlike SPSC, the MPSC bulk path cannot use `memcpy`: the slot layout is `{sequence, data}` per element, so publication is per-slot and the gain comes only from amortizing the head CAS. That is why 2P/256 reaches 666 M items/s where SPSC reaches 2 839. This is also the one place where **Join (Shm)** is consistently behind **Join (Local)**, by 9% to 29%.

### MPMC

![MPMC Throughput](plots/mpmc_throughput.png)

Throughput, M items/s:

| P/C | Join (Local) | Join (Shm) | Rigtorp | Moodycamel | Boost |
|---|---|---|---|---|---|
| 1/1 | 32.1 | **32.7** | 31.3 | 8.2 | 6.9 |
| 2/2 | **26.3** | 26.0 | 24.4 | 11.4 | 5.3 |
| 4/4 | **18.5** | 18.1 | 15.2 | 6.5 | 2.6 |
| 8/8 | **12.6** | 12.4 | 10.2 | 9.5 | 1.8 |
| 4/1 | 18.8 | **19.1** | 14.0 | 5.9 | 3.0 |
| 1/4 | **15.4** | 15.3 | 13.5 | 9.8 | 2.0 |

**Join** takes all six configurations, though its lead over **Rigtorp** is thin at low contention (1.04x at 1P/1C) and only becomes clear as threads are added (1.36x at 4P/1C). **Boost** is 4.7x to 7.7x behind, the price of its `fixed_sized<true>` lock-freedom guarantee. The 8P/8C row carries `smt=1`: 16 threads on 8 physical cores, every pair sharing a core.

![MPMC Latency Distribution](plots/mpmc_latency_dist.png)
![MPMC Latency Percentiles](plots/mpmc_latency_pct.png)

Latency at 1M items/s offered, P50 / P99 in ns, `dropped` = 0 everywhere:

| P/C | Join (Local) | Join (Shm) | Rigtorp | Moodycamel | Boost |
|---|---|---|---|---|---|
| 1/1 | **70** / 100 | **70** / 100 | **70** / 83 | 170 / 297 | 121 / 133 |
| 2/2 | 116 / 145 | 114 / 144 | **100** / 137 | 252 / 367 | 261 / 361 |
| 4/4 | 299 / 458 | 254 / 337 | **234** / 367 | 684 / 1 178 | 920 / 1 705 |
| 4/1 | 230 / 473 | 252 / 499 | **221** / 456 | 506 / 1 320 | 712 / 1 361 |
| 1/4 | 270 / 361 | 270 / 290 | **160** / 290 | 409 / 661 | 501 / 862 |

Here the ranking reverses: **Rigtorp** has the lowest P50 in four of five configurations, tying with **Join** only at 1P/1C. The margin is 4% to 9% at 2P/2C, 4P/4C and 4P/1C — inside or close to the `cv` of these measurements — but 1.7x at 1P/4C, which is outside it. **Join** wins the throughput table and **Rigtorp** the latency table; they are not the same trade-off.

**Moodycamel** is 1.5x to 2.6x behind the leader, **Boost** 1.7x to 3.9x.

The 70 ns one-way figure at 1P/1C is consistent with the 179 ns SPSC round-trip, which is the cross-check that this benchmark measures what it claims to.

![MPMC Bulk Throughput](plots/mpmc_bulk.png)

Bulk throughput, M items/s:

| P/C/batch | Join (Local) | Join (Shm) | Moodycamel |
|---|---|---|---|
| 1/1/8 | **105.4** | 87.3 | 54.4 |
| 1/1/64 | **289.5** | 193.7 | 187.8 |
| 1/1/256 | **437.9** | 236.8 | 168.2 |
| 2/2/64 | **291.3** | **291.3** | 155.1 |
| 2/2/256 | **402.1** | 400.8 | 149.1 |
| 4/4/64 | 116.0 | **117.6** | 67.8 |
| 4/4/256 | 163.4 | **163.3** | 68.3 |

**Join** leads everywhere, by 1.5x to 2.7x. **Moodycamel** stops gaining past batch 64 and loses ground at 256 in three of four configurations. **Join (Shm)** trails **Join (Local)** noticeably at 1P/1C only, by up to 1.85x; from 2P/2C on the two are identical.

### Conclusion

- **Join (Local)** and **Join (Shm)** win throughput in all 12 MPSC and MPMC configurations and in SPSC at capacity 256, win MPSC latency outright, and are the only library with a native bulk API for all three patterns. The two larger SPSC capacities are too dispersed between runs to rank anyone, and at 4096 **Moodycamel** does not resolve to a single value at all. The two backends track each other within a few percent except in MPSC bulk and MPMC 1P/1C bulk, where the shared-memory variant gives up 9% to 45%.
- **Rigtorp** loses on throughput but **wins MPMC latency in four of five configurations**. Most of those margins sit inside the noise floor; the 1P/4C one, at 1.7x, does not. It has no MPSC queue and no bulk API, so it covers less ground, but on latency alone it is the reference.
- **Moodycamel** is 2.2x to 3.8x behind on MPSC throughput, 1.5x to 2.6x behind on latency, and its bulk path stops scaling past batch 64. Its block-based design targets a different trade-off than the single-ring queues it is compared against here.
- **Boost** trails on throughput (4.7x to 7.7x in MPMC) and on latency, and offers neither MPSC nor bulk. What it does offer is an explicit lock-freedom guarantee through `fixed_sized<true>` with no dynamic allocation.
- **Contention costs more than library choice**: every MPMC library loses half its throughput between 1P/1C and 4P/4C. Library choice matters most at low to moderate contention.
- **Batching matters more than everything else**: **Join** SPSC goes from 175 to 2 839 M items/s between single-item and batch-256 transfers. That 16.2x dwarfs every difference between libraries measured here.

---

## License

Licensed under the [MIT License](LICENSE).
