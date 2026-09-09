#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

extern "C" {
#include "hashmap.h"
}

namespace {
/*
 * The C hashmap type we're benchmarking.
 */
typedef HashMap(uint64_t, uint64_t) U64Map;
} // namespace

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

static uint64_t mix64(uint64_t x) {
  x += 0x9e3779b97f4a7c15ULL;

  x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;

  return x ^ (x >> 31U);
}

static std::vector<uint64_t> make_present_keys(size_t count) {
  std::vector<uint64_t> keys;
  keys.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    keys.push_back(mix64(static_cast<uint64_t>(i) * 2));
  }

  return keys;
}

static std::vector<uint64_t> make_missing_keys(size_t count) {
  std::vector<uint64_t> keys;
  keys.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    keys.push_back(mix64((static_cast<uint64_t>(i) * 2) + 1));
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

namespace {
struct Fnv1aU64 {
  size_t operator()(uint64_t value) const noexcept {
    constexpr uint64_t offset_basis = 0xcbf29ce484222325ULL;
    constexpr uint64_t prime = 0x00000100000001b3ULL;

    uint64_t hash = offset_basis;

    const auto *bytes = reinterpret_cast<const unsigned char *>(&value);

    for (size_t i = 0; i < sizeof(value); ++i) {
      hash ^= bytes[i];
      hash *= prime;
    }

    return static_cast<size_t>(hash);
  }
};
} // namespace
using StdFnvMap = std::unordered_map<uint64_t, uint64_t, Fnv1aU64>;

using StdDefaultMap = std::unordered_map<uint64_t, uint64_t>;

/*
 * --------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------
 */

static void init_map(U64Map *map) {
  hashmap_init_with_algo(map, Fnv1a, &hashmap_hash_u64, &hashmap_equal_bytes);
}

/*
 * hashmap_reserve() is assumed to reserve backing-table capacity rather
 * than "number of elements".
 *
 * Since your growth threshold is 80%, reserving exactly N buckets would
 * still grow before N insertions.
 */
static size_t capacity_for_elements(size_t elements) {
  return (((elements * 100) + HASHMAP_LOAD_FACTOR_PERCENT - 1) /
          HASHMAP_LOAD_FACTOR_PERCENT) +
         1;
}

/*
 * Standard benchmark sizes:
 *
 *   ~L1-ish
 *   ~L2/L3-ish
 *   substantially larger
 */

static void MapSizes(benchmark::Benchmark *b) {
  b->Arg(1U << 10U);
  b->Arg(1U << 14U);
  b->Arg(1U << 18U);
}

static void LoadFactors(benchmark::Benchmark *b) {
  b->Arg(25);
  b->Arg(50);
  b->Arg(55);
  b->Arg(60);
  b->Arg(70);
  b->Arg(80);
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

static void BM_HashMap_InsertReserved(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  for (auto _ : state) {
    (void)_;
    state.PauseTiming();

    U64Map map{};
    init_map(&map);

    hashmap_reserve(&map, capacity_for_elements(count));

    state.ResumeTiming();

    for (const auto key : keys) {
      hashmap_put(&map, key, key);
    }

    benchmark::ClobberMemory();

    state.PauseTiming();
    hashmap_free(&map);
    state.ResumeTiming();
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

/*
 * ==========================================================================
 * INSERT — GROWING
 * ==========================================================================
 *
 * Initial map construction is excluded, but resizing caused by insertion
 * remains inside the timed region.
 */

static void BM_HashMap_InsertGrowing(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  for (auto _ : state) {
    (void)_;
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

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

/*
 * ==========================================================================
 * LOOKUP HIT
 * ==========================================================================
 */

static void BM_HashMap_LookupHit(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  U64Map map{};
  init_map(&map);

  hashmap_reserve(&map, capacity_for_elements(count));

  for (const auto key : keys) {
    hashmap_put(&map, key, key);
  }

  for (auto _ : state) {
    (void)_;
    for (const auto key : keys) {
      auto *value = hashmap_get(&map, key);

      benchmark::DoNotOptimize(value);

      if (value != nullptr) {
        benchmark::DoNotOptimize(*value);
      }
    }
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));

  hashmap_free(&map);
}

/*
 * ==========================================================================
 * LOOKUP MISS
 * ==========================================================================
 */

static void BM_HashMap_LookupMiss(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));

  const auto present = make_present_keys(count);
  const auto missing = make_missing_keys(count);

  U64Map map{};
  init_map(&map);

  hashmap_reserve(&map, capacity_for_elements(count));

  for (const auto key : present) {
    hashmap_put(&map, key, key);
  }

  for (auto _ : state) {
    (void)_;
    for (const auto key : missing) {
      auto *value = hashmap_get(&map, key);

      benchmark::DoNotOptimize(value);
    }
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));

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

static void BM_HashMap_Erase(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  for (auto _ : state) {
    (void)_;
    state.PauseTiming();

    U64Map map{};
    init_map(&map);

    hashmap_reserve(&map, capacity_for_elements(count));

    for (const auto key : keys) {
      hashmap_put(&map, key, key);
    }

    state.ResumeTiming();

    for (const auto key : keys) {
      bool removed = hashmap_remove(&map, key);
      benchmark::DoNotOptimize(removed);
    }

    state.PauseTiming();
    hashmap_free(&map);
    state.ResumeTiming();
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
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
static void BM_Std_InsertReserved(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  for (auto _ : state) {
    (void)_;
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

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

template <typename Map>
static void BM_Std_InsertGrowing(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  for (auto _ : state) {
    (void)_;
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

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

template <typename Map> static void BM_Std_LookupHit(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  Map map;
  map.reserve(count);

  for (const auto key : keys) {
    map.emplace(key, key);
  }

  for (auto _ : state) {
    (void)_;
    for (const auto key : keys) {
      auto it = map.find(key);

      benchmark::DoNotOptimize(it);

      if (it != map.end()) {
        benchmark::DoNotOptimize(it->second);
      }
    }
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

template <typename Map> static void BM_Std_LookupMiss(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));

  const auto present = make_present_keys(count);
  const auto missing = make_missing_keys(count);

  Map map;
  map.reserve(count);

  for (const auto key : present) {
    map.emplace(key, key);
  }

  for (auto _ : state) {
    (void)_;
    for (const auto key : missing) {
      auto it = map.find(key);

      benchmark::DoNotOptimize(it);
    }
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

template <typename Map> static void BM_Std_Erase(benchmark::State &state) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto keys = make_present_keys(count);

  for (auto _ : state) {
    (void)_;
    state.PauseTiming();

    Map map;
    map.reserve(count);

    for (const auto key : keys) {
      map.emplace(key, key);
    }

    state.ResumeTiming();

    for (const auto key : keys) {
      auto removed = map.erase(key);
      benchmark::DoNotOptimize(removed);
    }

    state.PauseTiming();
    map.clear();
    state.ResumeTiming();
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
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

static void BM_HashMap_LoadFactor_Hit(benchmark::State &state) {
  const auto requested_load = static_cast<size_t>(state.range(0));

  constexpr size_t requested_capacity = 1U << 16U;

  U64Map map{};
  init_map(&map);

  hashmap_reserve(&map, requested_capacity);

  const size_t capacity = hashmap_capacity(&map);
  const size_t count = capacity * requested_load / 100;

  const auto keys = make_present_keys(count);

  for (const auto key : keys) {
    hashmap_put(&map, key, key);
  }

  for (auto _ : state) {
    (void)_;
    for (const auto key : keys) {
      auto *value = hashmap_get(&map, key);

      benchmark::DoNotOptimize(value);

      if (value != nullptr) {
        benchmark::DoNotOptimize(*value);
      }
    }
  }

  state.counters["load"] =
      static_cast<double>(count) / static_cast<double>(capacity);

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));

  hashmap_free(&map);
}

static void BM_HashMap_LoadFactor_Miss(benchmark::State &state) {
  const auto requested_load = static_cast<size_t>(state.range(0));

  constexpr size_t requested_capacity = 1U << 16U;

  U64Map map{};
  init_map(&map);

  hashmap_reserve(&map, requested_capacity);

  const size_t capacity = hashmap_capacity(&map);
  const size_t count = capacity * requested_load / 100;

  const auto present = make_present_keys(count);
  const auto missing = make_missing_keys(count);

  for (const auto key : present) {
    hashmap_put(&map, key, key);
  }

  for (auto _ : state) {
    (void)_;
    for (const auto key : missing) {
      auto *value = hashmap_get(&map, key);

      benchmark::DoNotOptimize(value);
    }
  }

  state.counters["load"] =
      static_cast<double>(count) / static_cast<double>(capacity);

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));

  hashmap_free(&map);
}

static void BM_HashMap_ConfiguredLoadFactor_Insert(benchmark::State &state) {
  const auto load_factor = static_cast<size_t>(state.range(0));
  const auto count = static_cast<size_t>(1U << 14U);
  const auto keys = make_present_keys(count);

  for (auto _ : state) {
    (void)_;
    state.PauseTiming();

    U64Map map{};
    init_map(&map);
    hashmap_set_load_factor_percent(&map, load_factor);

    state.ResumeTiming();

    for (const auto key : keys) {
      hashmap_put(&map, key, key);
    }

    benchmark::ClobberMemory();

    state.PauseTiming();
    hashmap_free(&map);
    state.ResumeTiming();
  }

  state.counters["load_factor"] = static_cast<double>(load_factor);
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

#ifdef HASHMAP_ENABLE_STATS

static void BM_HashMap_Stats(benchmark::State &state,
                             const HashMapStats &totals) {

  const double iterations = static_cast<double>(state.iterations());
  state.counters["hashes/op"] =
      static_cast<double>(totals.hash_calculations) / iterations;
  state.counters["insert-probes/put"] =
      static_cast<double>(totals.insert_probes) / (double)totals.put_calls;
  state.counters["swaps/put"] =
      static_cast<double>(totals.insert_swaps) / (double)totals.put_calls;
  state.counters["lookup-probes/lookup"] =
      static_cast<double>(totals.lookup_probes) / (double)totals.lookup_calls;
  state.counters["remove-probes/remove"] =
      static_cast<double>(totals.remove_probes) / (double)totals.remove_calls;
  state.counters["resizes/iteration"] =
      static_cast<double>(totals.resize_count) / iterations;
}

static void
BM_HashMap_ConfiguredLoadFactor_Insert_Stats(benchmark::State &state) {
  const auto load_factor = static_cast<size_t>(state.range(0));
  const auto count = static_cast<size_t>(1U << 14U);
  const auto keys = make_present_keys(count);
  HashMapStats totals{};
  size_t capacity{};

  for (auto _ : state) {
    (void)_;
    state.PauseTiming();

    U64Map map{};
    init_map(&map);
    hashmap_set_load_factor_percent(&map, load_factor);

    state.ResumeTiming();

    for (const auto key : keys) {
      hashmap_put(&map, key, key);
    }

    benchmark::ClobberMemory();

    state.PauseTiming();
    const HashMapStats stats = hashmap_stats(&map);
    totals.hash_calculations += stats.hash_calculations;
    totals.put_calls += stats.put_calls;
    totals.insert_probes += stats.insert_probes;
    totals.insert_swaps += stats.insert_swaps;
    totals.lookup_calls += stats.lookup_calls;
    totals.lookup_probes += stats.lookup_probes;
    totals.remove_calls += stats.remove_calls;
    totals.remove_probes += stats.remove_probes;
    totals.resize_count += stats.resize_count;
    capacity += hashmap_capacity(&map);

    hashmap_free(&map);

    state.ResumeTiming();
  }

  state.counters["load_factor"] = static_cast<double>(load_factor);
  BM_HashMap_Stats(state, totals);
  state.counters["capacity"] =
      static_cast<double>(capacity) / static_cast<double>(state.iterations());
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}

static void BM_HashMap_Stats_inner(benchmark::State &state, auto init_map_fn) {
  const auto count = static_cast<size_t>(state.range(0));
  const auto present = make_present_keys(count);
  const auto missing = make_missing_keys(count);
  HashMapStats totals{};

  for (auto _ : state) {
    (void)_;
    state.PauseTiming();

    U64Map map{};
    init_map_fn(&map);
    hashmap_reserve(&map, capacity_for_elements(count));
    hashmap_stats_reset(&map);

    state.ResumeTiming();

    for (const auto key : present) {
      hashmap_put(&map, key, key);
    }
    for (const auto key : present) {
      benchmark::DoNotOptimize(hashmap_get(&map, key));
    }
    for (const auto key : missing) {
      benchmark::DoNotOptimize(hashmap_get(&map, key));
    }
    for (const auto key : present) {
      benchmark::DoNotOptimize(hashmap_remove(&map, key));
    }

    state.PauseTiming();

    const HashMapStats stats = hashmap_stats(&map);
    totals.hash_calculations += stats.hash_calculations;
    totals.put_calls += stats.put_calls;
    totals.insert_probes += stats.insert_probes;
    totals.insert_swaps += stats.insert_swaps;
    totals.lookup_calls += stats.lookup_calls;
    totals.lookup_probes += stats.lookup_probes;
    totals.remove_calls += stats.remove_calls;
    totals.remove_probes += stats.remove_probes;
    totals.resize_count += stats.resize_count;

    hashmap_free(&map);
    state.ResumeTiming();
  }
  BM_HashMap_Stats(state, totals);
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(count));
}
static void BM_HashMap_StatsFnv1a(benchmark::State &state) {
  BM_HashMap_Stats_inner(state, [](U64Map *map) {
    hashmap_init_with_algo(map, Fnv1a, &hashmap_hash_u64, &hashmap_equal_bytes);
  });
}

static void BM_HashMap_StatsSipHash(benchmark::State &state) {
  BM_HashMap_Stats_inner(state, [](U64Map *map) {
    hashmap_init_with_algo(map, SipHash, &hashmap_hash_u64,
                           &hashmap_equal_bytes);
  });
}

static void BM_HashMap_InsertAtOccupancy_Stats(benchmark::State &state) {
  const auto occupancy_percent = static_cast<size_t>(state.range(0));

  constexpr size_t capacity = 1U << 15U; // 32768
  constexpr size_t max_load_factor = 95;
  constexpr size_t insert_count = 256;

  const size_t initial_count = (capacity * occupancy_percent) / 100U;

  const auto keys = make_present_keys(initial_count + insert_count);

  HashMapStats totals{};

  for (auto _ : state) {
    (void)_;

    state.PauseTiming();

    U64Map map{};
    init_map(&map);

    hashmap_set_load_factor_percent(&map, max_load_factor);
    hashmap_reserve(&map, capacity);

    for (size_t i = 0; i < initial_count; ++i) {
      const auto key = keys[i];
      hashmap_put(&map, key, key);
    }

    assert(hashmap_capacity(&map) == capacity);

    hashmap_stats_reset(&map);

    state.ResumeTiming();

    for (size_t i = 0; i < insert_count; ++i) {
      const auto key = keys[initial_count + i];
      hashmap_put(&map, key, key);
    }

    benchmark::ClobberMemory();

    state.PauseTiming();

    const auto stats = hashmap_stats(&map);

    totals.hash_calculations += stats.hash_calculations;
    totals.put_calls += stats.put_calls;
    totals.insert_probes += stats.insert_probes;
    totals.insert_swaps += stats.insert_swaps;
    totals.lookup_calls += stats.lookup_calls;
    totals.lookup_probes += stats.lookup_probes;
    totals.remove_calls += stats.remove_calls;
    totals.remove_probes += stats.remove_probes;
    totals.resize_count += stats.resize_count;

    assert(hashmap_capacity(&map) == capacity);

    hashmap_free(&map);

    state.ResumeTiming();
  }

  state.counters["occupancy"] = static_cast<double>(occupancy_percent);

  state.counters["capacity"] = static_cast<double>(capacity);

  state.counters["hashes/op"] = static_cast<double>(totals.hash_calculations) /
                                static_cast<double>(totals.put_calls);

  state.counters["insert-probes/put"] =
      static_cast<double>(totals.insert_probes) /
      static_cast<double>(totals.put_calls);

  state.counters["swaps/put"] = static_cast<double>(totals.insert_swaps) /
                                static_cast<double>(totals.put_calls);

  state.counters["resizes/iteration"] =
      static_cast<double>(totals.resize_count) /
      static_cast<double>(state.iterations());

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(insert_count));
}
#endif

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
BENCHMARK(BM_HashMap_ConfiguredLoadFactor_Insert)->Apply(LoadFactors);

#ifdef HASHMAP_ENABLE_STATS
BENCHMARK(BM_HashMap_StatsFnv1a)->Arg(1U << 14U);
BENCHMARK(BM_HashMap_StatsSipHash)->Arg(1U << 14U);
BENCHMARK(BM_HashMap_ConfiguredLoadFactor_Insert_Stats)->Apply(LoadFactors);
BENCHMARK(BM_HashMap_InsertAtOccupancy_Stats)
    ->Apply(LoadFactors)
    ->Iterations(1000);
#endif
