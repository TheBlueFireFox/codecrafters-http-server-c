#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

extern "C" {
#include "hashmap.h"
}

/*
 * The C hashmap type we're benchmarking.
 */
typedef HashMap(uint64_t, uint64_t) U64Map;

/*
 * --------------------------------------------------------------------------
 * Deterministic key generation
 * --------------------------------------------------------------------------
 *
 * SplitMix64's mixing function is a permutation of uint64_t, so using
 *
 *   mix(2 * i)
 *   mix(2 * i + 1)
 *
 * gives us two disjoint sets:
 *
 *   present keys
 *   missing keys
 *
 * No RNG work happens inside the timed benchmark.
 */

static uint64_t mix64(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;

    x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;

    return x ^ (x >> 31U);
}

static std::vector<uint64_t> make_present_keys(size_t count)
{
    std::vector<uint64_t> keys;
    keys.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        keys.push_back(mix64(static_cast<uint64_t>(i) * 2));
    }

    return keys;
}

static std::vector<uint64_t> make_missing_keys(size_t count)
{
    std::vector<uint64_t> keys;
    keys.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        keys.push_back(mix64(static_cast<uint64_t>(i) * 2 + 1));
    }

    return keys;
}

/*
 * --------------------------------------------------------------------------
 * std::unordered_map FNV-1a hasher
 * --------------------------------------------------------------------------
 *
 * Your hashmap_hash_u64() feeds the raw uint64_t bytes into FNV-1a.
 *
 * This deliberately does exactly the same thing so:
 *
 *     HashMap + FNV
 *
 * can be compared against
 *
 *     unordered_map + FNV
 *
 * separately from unordered_map's default std::hash<uint64_t>.
 */

struct Fnv1aU64 {
    size_t operator()(uint64_t value) const noexcept
    {
        constexpr uint64_t offset_basis = 0xcbf29ce484222325ULL;
        constexpr uint64_t prime = 0x00000100000001b3ULL;

        uint64_t hash = offset_basis;

        const auto *bytes =
            reinterpret_cast<const unsigned char *>(&value);

        for (size_t i = 0; i < sizeof(value); ++i) {
            hash ^= bytes[i];
            hash *= prime;
        }

        return static_cast<size_t>(hash);
    }
};

using StdFnvMap =
    std::unordered_map<uint64_t, uint64_t, Fnv1aU64>;

using StdDefaultMap =
    std::unordered_map<uint64_t, uint64_t>;

/*
 * --------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------
 */

static void init_map(U64Map *map)
{
    hashmap_init(
        map,
        &hashmap_hash_u64,
        &hashmap_equal_bytes
    );
}

/*
 * hashmap_reserve() is assumed to reserve backing-table capacity rather
 * than "number of elements".
 *
 * Since your growth threshold is 80%, reserving exactly N buckets would
 * still grow before N insertions.
 */
static size_t capacity_for_elements(size_t elements)
{
    return
        (elements * 100 + HASHMAP_LOAD_FACTOR_PERCENT - 1) /
            HASHMAP_LOAD_FACTOR_PERCENT
        + 1;
}

/*
 * Standard benchmark sizes:
 *
 *   ~L1-ish
 *   ~L2/L3-ish
 *   substantially larger
 */
static void MapSizes(benchmark::internal::Benchmark *benchmark)
{
    benchmark->Arg(1U << 10U);  // 1,024
    benchmark->Arg(1U << 14U);  // 16,384
    benchmark->Arg(1U << 18U);  // 262,144
}

static void LoadFactors(benchmark::internal::Benchmark *benchmark)
{
    /*
     * Your map resizes at 80%, so 79% is the interesting upper bound
     * without deliberately bypassing the public API.
     */
    benchmark->Arg(25);
    benchmark->Arg(50);
    benchmark->Arg(70);
    benchmark->Arg(79);
}

/*
 * ==========================================================================
 * INSERT — RESERVED
 * ==========================================================================
 *
 * Allocation/reserve is outside the timed region.
 *
 * This is mostly measuring:
 *
 *   hash
 *   probe
 *   Robin Hood insertion
 */

static void BM_HashMap_InsertReserved(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    for (auto _ : state) {
        state.PauseTiming();

        U64Map map{};
        init_map(&map);

        hashmap_reserve(
            &map,
            capacity_for_elements(count)
        );

        state.ResumeTiming();

        for (const auto key : keys) {
            hashmap_put(&map, key, key);
        }

        benchmark::ClobberMemory();

        state.PauseTiming();
        hashmap_free(&map);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

/*
 * ==========================================================================
 * INSERT — GROWING
 * ==========================================================================
 *
 * Initial map construction is excluded, but resizing caused by insertion
 * remains inside the timed region.
 */

static void BM_HashMap_InsertGrowing(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    for (auto _ : state) {
        state.PauseTiming();

        U64Map map{};
        init_map(&map);

        state.ResumeTiming();

        for (const auto key : keys) {
            hashmap_put(&map, key, key);
        }

        benchmark::ClobberMemory();

        state.PauseTiming();
        hashmap_free(&map);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

/*
 * ==========================================================================
 * LOOKUP HIT
 * ==========================================================================
 */

static void BM_HashMap_LookupHit(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    U64Map map{};
    init_map(&map);

    hashmap_reserve(
        &map,
        capacity_for_elements(count)
    );

    for (const auto key : keys) {
        hashmap_put(&map, key, key);
    }

    for (auto _ : state) {
        for (const auto key : keys) {
            auto *value = hashmap_get(&map, key);

            benchmark::DoNotOptimize(value);

            if (value != nullptr) {
                benchmark::DoNotOptimize(*value);
            }
        }
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );

    hashmap_free(&map);
}

/*
 * ==========================================================================
 * LOOKUP MISS
 * ==========================================================================
 */

static void BM_HashMap_LookupMiss(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));

    const auto present = make_present_keys(count);
    const auto missing = make_missing_keys(count);

    U64Map map{};
    init_map(&map);

    hashmap_reserve(
        &map,
        capacity_for_elements(count)
    );

    for (const auto key : present) {
        hashmap_put(&map, key, key);
    }

    for (auto _ : state) {
        for (const auto key : missing) {
            auto *value = hashmap_get(&map, key);

            benchmark::DoNotOptimize(value);
        }
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );

    hashmap_free(&map);
}

/*
 * ==========================================================================
 * ERASE
 * ==========================================================================
 *
 * The map has to be reconstructed each iteration because erase changes it.
 * Reconstruction is outside the timed region.
 */

static void BM_HashMap_Erase(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    for (auto _ : state) {
        state.PauseTiming();

        U64Map map{};
        init_map(&map);

        hashmap_reserve(
            &map,
            capacity_for_elements(count)
        );

        for (const auto key : keys) {
            hashmap_put(&map, key, key);
        }

        state.ResumeTiming();

        for (const auto key : keys) {
            const bool removed = hashmap_remove(&map, key);
            benchmark::DoNotOptimize(removed);
        }

        state.PauseTiming();
        hashmap_free(&map);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

/*
 * ==========================================================================
 * std::unordered_map implementations
 * ==========================================================================
 *
 * Templates let us run exactly the same workload against:
 *
 *   std::hash<uint64_t>
 *   FNV-1a
 */

template <typename Map>
static void BM_Std_InsertReserved(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    for (auto _ : state) {
        state.PauseTiming();

        Map map;
        map.reserve(count);

        state.ResumeTiming();

        for (const auto key : keys) {
            map.emplace(key, key);
        }

        benchmark::ClobberMemory();

        state.PauseTiming();
        map.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

template <typename Map>
static void BM_Std_InsertGrowing(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    for (auto _ : state) {
        state.PauseTiming();

        Map map;

        state.ResumeTiming();

        for (const auto key : keys) {
            map.emplace(key, key);
        }

        benchmark::ClobberMemory();

        state.PauseTiming();
        map.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

template <typename Map>
static void BM_Std_LookupHit(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    Map map;
    map.reserve(count);

    for (const auto key : keys) {
        map.emplace(key, key);
    }

    for (auto _ : state) {
        for (const auto key : keys) {
            const auto it = map.find(key);

            benchmark::DoNotOptimize(it);

            if (it != map.end()) {
                benchmark::DoNotOptimize(it->second);
            }
        }
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

template <typename Map>
static void BM_Std_LookupMiss(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));

    const auto present = make_present_keys(count);
    const auto missing = make_missing_keys(count);

    Map map;
    map.reserve(count);

    for (const auto key : present) {
        map.emplace(key, key);
    }

    for (auto _ : state) {
        for (const auto key : missing) {
            const auto it = map.find(key);

            benchmark::DoNotOptimize(it);
        }
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

template <typename Map>
static void BM_Std_Erase(benchmark::State &state)
{
    const auto count = static_cast<size_t>(state.range(0));
    const auto keys = make_present_keys(count);

    for (auto _ : state) {
        state.PauseTiming();

        Map map;
        map.reserve(count);

        for (const auto key : keys) {
            map.emplace(key, key);
        }

        state.ResumeTiming();

        for (const auto key : keys) {
            const auto removed = map.erase(key);
            benchmark::DoNotOptimize(removed);
        }

        state.PauseTiming();
        map.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );
}

/*
 * ==========================================================================
 * LOAD FACTOR
 * ==========================================================================
 *
 * Fixed backing capacity, varied population.
 *
 * This is intentionally just your hashmap. std::unordered_map uses a very
 * different bucket organization, so "70% full" doesn't describe equivalent
 * physical layouts.
 */

static void BM_HashMap_LoadFactor_Hit(benchmark::State &state)
{
    const auto requested_load =
        static_cast<size_t>(state.range(0));

    constexpr size_t requested_capacity = 1U << 16U;

    U64Map map{};
    init_map(&map);

    hashmap_reserve(&map, requested_capacity);

    const size_t capacity = hashmap_capacity(&map);
    const size_t count =
        capacity * requested_load / 100;

    const auto keys = make_present_keys(count);

    for (const auto key : keys) {
        hashmap_put(&map, key, key);
    }

    for (auto _ : state) {
        for (const auto key : keys) {
            auto *value = hashmap_get(&map, key);

            benchmark::DoNotOptimize(value);

            if (value != nullptr) {
                benchmark::DoNotOptimize(*value);
            }
        }
    }

    state.counters["load"] =
        static_cast<double>(count) /
        static_cast<double>(capacity);

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );

    hashmap_free(&map);
}

static void BM_HashMap_LoadFactor_Miss(benchmark::State &state)
{
    const auto requested_load =
        static_cast<size_t>(state.range(0));

    constexpr size_t requested_capacity = 1U << 16U;

    U64Map map{};
    init_map(&map);

    hashmap_reserve(&map, requested_capacity);

    const size_t capacity = hashmap_capacity(&map);
    const size_t count =
        capacity * requested_load / 100;

    const auto present = make_present_keys(count);
    const auto missing = make_missing_keys(count);

    for (const auto key : present) {
        hashmap_put(&map, key, key);
    }

    for (auto _ : state) {
        for (const auto key : missing) {
            auto *value = hashmap_get(&map, key);

            benchmark::DoNotOptimize(value);
        }
    }

    state.counters["load"] =
        static_cast<double>(count) /
        static_cast<double>(capacity);

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(count)
    );

    hashmap_free(&map);
}

/*
 * ==========================================================================
 * Registration
 * ==========================================================================
 */

BENCHMARK(BM_HashMap_InsertReserved)->Apply(MapSizes);
BENCHMARK(BM_HashMap_InsertGrowing)->Apply(MapSizes);
BENCHMARK(BM_HashMap_LookupHit)->Apply(MapSizes);
BENCHMARK(BM_HashMap_LookupMiss)->Apply(MapSizes);
BENCHMARK(BM_HashMap_Erase)->Apply(MapSizes);

BENCHMARK_TEMPLATE(BM_Std_InsertReserved, StdFnvMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_InsertGrowing, StdFnvMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_LookupHit, StdFnvMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_LookupMiss, StdFnvMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_Erase, StdFnvMap)->Apply(MapSizes);

BENCHMARK_TEMPLATE(BM_Std_InsertReserved, StdDefaultMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_InsertGrowing, StdDefaultMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_LookupHit, StdDefaultMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_LookupMiss, StdDefaultMap)->Apply(MapSizes);
BENCHMARK_TEMPLATE(BM_Std_Erase, StdDefaultMap)->Apply(MapSizes);

BENCHMARK(BM_HashMap_LoadFactor_Hit)->Apply(LoadFactors);
BENCHMARK(BM_HashMap_LoadFactor_Miss)->Apply(LoadFactors);
