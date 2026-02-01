#!/usr/bin/env bash
# Automated MPSC queue benchmarking: run one benchmark per (queue_size, producer_count) pair,
# capture Tracy profiling per run, then visualize. Enables analyzing Tracy per producer/queue combo.
#
# Prerequisites:
#   - Build the project so build/benchmarks/benchmark_test exists
#   - perf on PATH (for TLB and cache miss events)
#   - tracy-capture on PATH
#   - tracy-csvexport on PATH (exports each .tracy to .csv)
#   - Python 3 with plotly (for scripts/visualize_benchmarks.py)
#
# Usage: run from project root, or pass project root as first argument.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${1:-$(cd "$SCRIPT_DIR/.." && pwd)}"
cd "$PROJECT_ROOT"

BENCHMARK_BIN="${PROJECT_ROOT}/build/benchmarks/benchmark_test"
RESULTS_DIR="${PROJECT_ROOT}/results"

# Queue sizes and producer counts supported by benchmark_test (see mpsc_queue_benchmark.cpp)
QUEUE_SIZES=(32 64 128 256 512 1024 2048 4096 8192 10240)
PRODUCER_COUNTS=(1 2 4 8 16 32 64)

if [[ ! -x "$BENCHMARK_BIN" ]]; then
    echo "Error: benchmark binary not found or not executable: $BENCHMARK_BIN"
    echo "Build the project first (e.g. ./build.sh or cmake --build build)."
    exit 1
fi

mkdir -p "$RESULTS_DIR"
export PATH=$PATH:~/.tools

if ! command -v perf &>/dev/null; then
    echo "perf not found; please install (e.g. linux-tools-generic) and add to PATH"
    exit 1
fi
if ! command -v tracy-capture &>/dev/null; then
    echo "tracy-capture not found; please install and add to PATH"
    exit 1
fi
if ! command -v tracy-csvexport &>/dev/null; then
    echo "tracy-csvexport not found; please install and add to PATH"
    exit 1
fi

echo "Running one benchmark per (queue_size, producer_count) pair."
echo "Queue sizes: ${QUEUE_SIZES[*]}"
echo "Producer counts: ${PRODUCER_COUNTS[*]}"
echo ""

for q in "${QUEUE_SIZES[@]}"; do
    for p in "${PRODUCER_COUNTS[@]}"; do
        echo "=============================================="
        echo "Queue size: $q  Producer count: $p"
        echo "=============================================="

        RESULTS_JSON="${RESULTS_DIR}/benchmark_results_q${q}_p${p}.json"
        TRACY_OUT="${RESULTS_DIR}/tracy_q${q}_p${p}.tracy"
        TRACY_CSV="${RESULTS_DIR}/tracy_q${q}_p${p}.csv"
        PERF_OUT="${RESULTS_DIR}/perf_q${q}_p${p}.txt"
        rm -f "$RESULTS_JSON"

        tracy-capture -o "$TRACY_OUT" -s 120 -f &
        TRACY_PID=$!
        sleep 2

        # TLB and cache miss events
        PERF_EVENTS="dTLB-load-misses,dTLB-store-misses,iTLB-load-misses,cache-misses,L1-dcache-load-misses,LLC-load-misses"
        if ! perf stat -e "$PERF_EVENTS" -o "$PERF_OUT" -- "$BENCHMARK_BIN" -q "$q" -p "$p" -r "$RESULTS_JSON"; then
            kill -INT "$TRACY_PID" 2>/dev/null || true
            wait "$TRACY_PID" 2>/dev/null || true
            echo "Benchmark failed for q=$q p=$p"
            exit 1
        fi

        kill -INT "$TRACY_PID" 2>/dev/null || true
        wait "$TRACY_PID" 2>/dev/null || true
        echo "Tracy trace saved to: $TRACY_OUT"

        if tracy-csvexport "$TRACY_OUT" > "$TRACY_CSV" 2>/dev/null; then
            echo "Tracy CSV exported to: $TRACY_CSV"
        else
            echo "Warning: tracy-csvexport failed for $TRACY_OUT"
        fi

        echo "Perf (TLB/cache misses) saved to: $PERF_OUT"
        cat "$PERF_OUT"

        if [[ ! -f "$RESULTS_JSON" ]]; then
            echo "Error: benchmark did not produce $RESULTS_JSON"
            exit 1
        fi
        echo ""
    done
done

HTML_OUT="${RESULTS_DIR}/benchmark_visualizations.html"
if ! python3 "$SCRIPT_DIR/visualize_benchmarks.py" -i "$RESULTS_DIR" -o "$HTML_OUT"; then
    echo "Visualization failed"
    exit 1
fi

echo "Done. Visualizations: $HTML_OUT"
echo "Results: ${RESULTS_DIR}/benchmark_results_q<Q>_p<P>.json"
echo "Tracy:   ${RESULTS_DIR}/tracy_q<Q>_p<P>.tracy / .csv"
echo "Perf:    ${RESULTS_DIR}/perf_q<Q>_p<P>.txt (TLB and cache misses)"
