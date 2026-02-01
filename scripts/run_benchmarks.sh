#!/bin/bash

# Automated MPSC queue benchmarking: run benchmark for all queue sizes,
# capture Tracy profiling, visualize each run, and remove benchmark_results.json after each run.
#
# Prerequisites:
#   - Build the project so build/benchmarks/benchmark_test exists
#   - tracy-capture on PATH (optional; skipped if not found)
#   - Python 3 with plotly (for scripts/visualize_benchmarks.py)
#
# Usage: run from project root, or pass project root as first argument.

set -euo pipefail

# Project root: script dir's parent, or first argument
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${1:-$(cd "$SCRIPT_DIR/.." && pwd)}"
cd "$PROJECT_ROOT"

BENCHMARK_BIN="${PROJECT_ROOT}/build/benchmarks/benchmark_test"
BENCHMARKS_DIR="${PROJECT_ROOT}/build/benchmarks"
RESULTS_DIR="${PROJECT_ROOT}/results"

# All queue sizes supported by benchmark_test -q (see mpsc_queue_benchmark.cpp)
QUEUE_SIZES=(32 64 128 256 512 1024 2048 4096 8192 10240)

if [[ ! -x "$BENCHMARK_BIN" ]]; then
    echo "Error: benchmark binary not found or not executable: $BENCHMARK_BIN"
    echo "Build the project first (e.g. ./build.sh or cmake --build build)."
    exit 1
fi

mkdir -p "$RESULTS_DIR"
mkdir -p "$BENCHMARKS_DIR"

# this is where tracy-capture is installed
export PATH=$PATH:~/.tools
if command -v tracy-capture &>/dev/null; then
    echo "tracy-capture found; Tracy profiling will be recorded for each run."
else
    echo "tracy-capture not found; please install tracy-capture locally and alias it to tracy-capture in your shell"
    exit 1
fi

for q in "${QUEUE_SIZES[@]}"; do
    echo "=============================================="
    echo "Queue size: $q"
    echo "=============================================="

    # Remove previous run's results so this run writes a clean file
    RESULTS_JSON="${RESULTS_DIR}/benchmark_results_q${q}.json"
    rm -f "$RESULTS_JSON"

    TRACY_PID=""
    TRACY_OUT="${RESULTS_DIR}/tracy_q${q}.tracy"
    tracy-capture -o "$TRACY_OUT" -s 120 -f &
    TRACY_PID=$!
    # Give tracy-capture a moment to listen
    sleep 2

    # Run the benchmark and capture its exit status
    "$BENCHMARK_BIN" -q "$q" -r "$RESULTS_JSON"
    BENCH_EXIT_CODE=$?

    if [[ -n "${TRACY_PID:-}" ]]; then
        kill -INT "$TRACY_PID" 2>/dev/null || true
        wait "$TRACY_PID" 2>/dev/null || true
    fi

    if [[ $BENCH_EXIT_CODE -ne 0 ]]; then
        echo "Benchmark failed (exit code $BENCH_EXIT_CODE) for queue size $q"
        exit 1
    fi

    if [[ -n "${TRACY_PID:-}" ]]; then
        kill -INT "$TRACY_PID" 2>/dev/null || true
        wait "$TRACY_PID" 2>/dev/null || true
        echo "Tracy trace saved to: ${BENCHMARKS_DIR}/tracy_q${q}.tracy"
    fi


    if [[ ! -f "$RESULTS_JSON" ]]; then
        echo "Error: benchmark did not produce $RESULTS_JSON"
        exit 1
    fi

done

HTML_OUT="${RESULTS_DIR}/benchmark_visualizations.html"
if ! python3 "$SCRIPT_DIR/visualize_benchmarks.py" -i "$RESULTS_DIR" -o "$HTML_OUT"; then
    echo "Visualization failed"
    exit 1
fi
echo "Visualization saved to: $HTML_OUT"
echo "All queue sizes finished. Visualizations are in ${RESULTS_DIR}/benchmark_visualizations.html"
echo "Tracy traces are in ${RESULTS_DIR}/tracy_q<SIZE>.tracy"
