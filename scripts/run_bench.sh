#!/bin/bash
set -e

BUILD_DIR="${BUILD_DIR:-build}"
RESULTS_DIR="results"
REPETITIONS="${REPETITIONS:-10}"

echo "=== Build ==="
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

mkdir -p "$RESULTS_DIR"

echo "=== Benchmarks ==="
run_bench() {
    local exe="$1"
    local out="$2"
    if [ -f "$exe" ]; then
        echo "  → $(basename "$exe")"
        "$exe" \
            --benchmark_format=json \
            --benchmark_out="$out" \
            --benchmark_repetitions="$REPETITIONS" \
            --benchmark_min_warmup_time=0.1 \
            --benchmark_report_aggregates_only=true \
            --benchmark_counters_tabular=true
    fi
}

run_bench "$BUILD_DIR/benchmarks/spsc/bench_spsc_throughput" "$RESULTS_DIR/spsc_throughput.json"
run_bench "$BUILD_DIR/benchmarks/spsc/bench_spsc_latency"    "$RESULTS_DIR/spsc_latency.json"
run_bench "$BUILD_DIR/benchmarks/spsc/bench_spsc_bulk"       "$RESULTS_DIR/spsc_bulk.json"
run_bench "$BUILD_DIR/benchmarks/mpsc/bench_mpsc_throughput" "$RESULTS_DIR/mpsc_throughput.json"
run_bench "$BUILD_DIR/benchmarks/mpsc/bench_mpsc_latency"    "$RESULTS_DIR/mpsc_latency.json"
run_bench "$BUILD_DIR/benchmarks/mpsc/bench_mpsc_bulk"       "$RESULTS_DIR/mpsc_bulk.json"
run_bench "$BUILD_DIR/benchmarks/mpmc/bench_mpmc_throughput" "$RESULTS_DIR/mpmc_throughput.json"
run_bench "$BUILD_DIR/benchmarks/mpmc/bench_mpmc_latency"    "$RESULTS_DIR/mpmc_latency.json"
run_bench "$BUILD_DIR/benchmarks/mpmc/bench_mpmc_bulk"       "$RESULTS_DIR/mpmc_bulk.json"

echo "=== Plots ==="
cd plots
python3 plot.py ../results/*.json
echo "Graphiques générés dans plots/"
