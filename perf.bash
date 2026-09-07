#!/usr/bin/env bash

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

BENCH="${BENCH:-./http-server-benchmark}"
CPU="${CPU:-4}"

FILTER="${FILTER:-BM_HashMap_LookupMiss/16384\$}"
SYMBOL="${SYMBOL:-hashmap_lookup}"

REPETITIONS="${REPETITIONS:-10}"

PERF_DATA="${PERF_DATA:-perf.data}"
PERF_REPORT="${PERF_REPORT:-perf-report.txt}"
PERF_ANNOTATE="${PERF_ANNOTATE:-perf-annotate.txt}"

# Optional:
#
#   FLAMEGRAPH_DIR=~/Programming/FlameGraph ./profile.sh
#
FLAMEGRAPH_DIR="/usr/bin"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

run_bench() {
    taskset -c "$CPU" "$BENCH" \
        --benchmark_filter="$FILTER" \
        "$@"
}

section() {
    echo
    echo "======================================================================"
    echo "$1"
    echo "======================================================================"
}

# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------

command -v perf >/dev/null || {
    echo "error: perf not found"
    exit 1
}

command -v taskset >/dev/null || {
    echo "error: taskset not found"
    exit 1
}

if [[ ! -x "$BENCH" ]]; then
    echo "error: benchmark executable not found: $BENCH"
    exit 1
fi

echo "Benchmark : $BENCH"
echo "Filter    : $FILTER"
echo "CPU       : $CPU"
echo "Symbol    : $SYMBOL"

# ---------------------------------------------------------------------------
# 1. Stable Google Benchmark baseline
# ---------------------------------------------------------------------------

section "Benchmark repetitions"

run_bench \
    --benchmark_repetitions="$REPETITIONS" \
    --benchmark_report_aggregates_only=true

# ---------------------------------------------------------------------------
# 2. Instructions / cycles
#
# Kept separate from the other counters to avoid perf multiplexing.
# ---------------------------------------------------------------------------

section "Cycles and instructions"

taskset -c "$CPU" perf stat -r 5 \
    -e cycles,instructions \
    "$BENCH" \
    --benchmark_filter="$FILTER"

# ---------------------------------------------------------------------------
# 3. Branch behavior
# ---------------------------------------------------------------------------

section "Branches"

taskset -c "$CPU" perf stat -r 5 \
    -e branches,branch-misses \
    "$BENCH" \
    --benchmark_filter="$FILTER"

# ---------------------------------------------------------------------------
# 4. L1 data-cache behavior
# ---------------------------------------------------------------------------

section "L1 data cache"

taskset -c "$CPU" perf stat -r 5 \
    -e L1-dcache-loads,L1-dcache-load-misses \
    "$BENCH" \
    --benchmark_filter="$FILTER"

# ---------------------------------------------------------------------------
# 5. Generic cache counters
# ---------------------------------------------------------------------------

section "Generic cache counters"

taskset -c "$CPU" perf stat -r 5 \
    -e cache-references,cache-misses \
    "$BENCH" \
    --benchmark_filter="$FILTER"

# ---------------------------------------------------------------------------
# 6. LLC counters
#
# These aren't supported identically on every CPU, so failure isn't fatal.
# ---------------------------------------------------------------------------

section "Last-level cache"

if taskset -c "$CPU" perf stat -r 5 \
    -e LLC-loads,LLC-load-misses \
    "$BENCH" \
    --benchmark_filter="$FILTER"
then
    :
else
    echo
    echo "LLC-loads/LLC-load-misses not supported by this PMU; skipping."
fi

# ---------------------------------------------------------------------------
# 7. Record cycle profile
#
# :pp requests more precise event attribution where supported.
#
# We use frame-pointer call graphs because profiling builds should preferably
# be compiled with:
#
#   -O2 -g -fno-omit-frame-pointer
# ---------------------------------------------------------------------------

section "Recording profile"

rm -f "$PERF_DATA"

taskset -c "$CPU" perf record \
    -o "$PERF_DATA" \
    -F 999 \
    -e cycles:u \
    -g \
    --call-graph fp \
    -- \
    "$BENCH" \
    --benchmark_filter="$FILTER" \
    --benchmark_min_time=2s

section "Checking profile"

SAMPLES="$(perf script -i "$PERF_DATA" 2>/dev/null | wc -l)"

echo "Recorded sample lines: $SAMPLES"

if [[ "$SAMPLES" -eq 0 ]]; then
    echo "error: perf recorded no samples"
    echo
    echo "Try:"
    echo "  perf record -F 999 -e cycles:u -- <program>"
    exit 1
fi

# ---------------------------------------------------------------------------
# 8. Text report
# ---------------------------------------------------------------------------

section "Perf report"

perf report \
    -i "$PERF_DATA" \
    --stdio \
    > "$PERF_REPORT"

head -n 50 "$PERF_REPORT"

echo
echo "Full report: $PERF_REPORT"

# ---------------------------------------------------------------------------
# 9. Annotate hot function
# ---------------------------------------------------------------------------

section "Annotated function: $SYMBOL"

if perf annotate \
    -i "$PERF_DATA" \
    --stdio \
    "$SYMBOL" \
    > "$PERF_ANNOTATE"
then
    cat "$PERF_ANNOTATE"
else
    echo "Could not annotate '$SYMBOL'."
fi

echo
echo "Annotation: $PERF_ANNOTATE"

# ---------------------------------------------------------------------------
# 10. Optional flame graph
# ---------------------------------------------------------------------------

if [[ -n "$FLAMEGRAPH_DIR" ]]; then

    section "Flame graph"

    STACKCOLLAPSE="$FLAMEGRAPH_DIR/stackcollapse-perf.pl"
    FLAMEGRAPH="$FLAMEGRAPH_DIR/flamegraph.pl"

    if [[ ! -x "$STACKCOLLAPSE" || ! -x "$FLAMEGRAPH" ]]; then
        echo "FlameGraph scripts not found in:"
        echo "  $FLAMEGRAPH_DIR"
        exit 1
    fi

    perf script -i "$PERF_DATA" > perf-script.txt

    "$STACKCOLLAPSE" \
        perf-script.txt \
        > perf-folded.txt

    "$FLAMEGRAPH" \
        perf-folded.txt \
        > flamegraph.svg

    echo "Created flamegraph.svg"
fi

section "Done"
