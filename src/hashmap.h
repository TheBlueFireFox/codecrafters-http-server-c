#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hashmap_hasher.h"
#include "utils.h"

/* HASHMAP */
#define HASHMAP_DEFAULT_CAPACITY 16
#define HASHMAP_LOAD_FACTOR_PERCENT 80

struct HashMapSlotQuery {
  size_t header_size;
  size_t header_alignment;
  size_t key_size;
  size_t key_alignment;
  size_t value_size;
  size_t value_alignment;
};

typedef struct HashMapSlotQuery HashMapSlotQuery;

struct HashMapSlotConfigurations {
  size_t key_offset;
  size_t value_offset;
  size_t slot_size;
};

typedef struct HashMapSlotConfigurations HashMapSlotConfigurations;

#define MAX(a, b) (a) < (b) ? (b) : (a)

HashMapSlotConfigurations hashmap_slot_config(struct HashMapSlotQuery *query);

#define HASHMAP_HASH_EMPTY 0

struct HashMapSlotHeader {
  Hash hash;
};

typedef struct HashMapSlotHeader HashMapSlotHeader;

struct HashMapSlot {
  HashMapSlotHeader *header;
  void *key;
  void *value;
};

typedef struct HashMapSlot HashMapSlot;

#ifdef HASHMAP_ENABLE_STATS
struct HashMapStats {
  size_t hash_calculations;
  size_t put_calls;
  size_t insert_probes;
  size_t insert_swaps;
  size_t lookup_calls;
  size_t lookup_probes;
  size_t remove_calls;
  size_t remove_probes;
  size_t resize_count;
};

typedef struct HashMapStats HashMapStats;
#endif

struct HashMapInternal {
  size_t len;
  size_t capacity;
  size_t mask_capacity;
  size_t grow_at;
  size_t load_factor_percent;
  HashMapAlgorithm hash_algo;
  HashMapHashFn hash_fn;
  HashMapEqFn eq_fn;
  uint8_t *algo_config;
  size_t key_size;
  size_t key_offset;
  size_t value_size;
  size_t value_offset;
  size_t slot_size;
#ifdef HASHMAP_ENABLE_STATS
  HashMapStats stats;
#endif
  // Data := HashMapSlotHeader KEY VALUE * capacity
  // padding it for alignment
  // ┌─────────────── entry 0 ─────────────────────────────────────────────────┐
  // │ hash │ occupied │ padding │ key bytes │ padding │ value bytes │ padding │
  // ├─────────────── entry 1 ─────────────────────────────────────────────────┤
  // │ hash │ occupied │ padding │ key bytes │ padding │ value bytes │ padding │
  // ├─────────────────────────────────────────────────────────────────────────┤
  // │ ...                                                                     │
  // └─────────────────────────────────────────────────────────────────────────┘
  uint8_t *data;
};

typedef struct HashMapInternal HashMapInternal;

#define HashMap(key_type, value_type)                                          \
  union {                                                                      \
    HashMapInternal internal;                                                  \
    /* NOLINTNEXTLINE bugprone-macro-parentheses */                            \
    key_type *key_payload;                                                     \
    /* NOLINTNEXTLINE bugprone-macro-parentheses */                            \
    value_type *value_payload;                                                 \
  }

#define hashmap_alignment_key(map) alignof(TYPEOF(*(map)->key_payload))

#define hashmap_alignment_value(map) alignof(TYPEOF(*(map)->value_payload))

#define hashmap_key_size(map) sizeof(*(map)->key_payload)

#define hashmap_value_size(map) sizeof(*(map)->value_payload)

#define hashmap_key(map) *(map)->key_payload

#define hashmap_value(map) *(map)->value_payload

#define hashmap_key_type(map) TYPEOF(*(map)->key_payload)

#define hashmap_value_type(map) TYPEOF(*(map)->value_payload)

#define hashmap_key_ptr(map) TYPEOF((map)->key_payload)

#define hashmap_value_ptr(map) TYPEOF((map)->value_payload)

void hashmap_init_with_algo_impl(HashMapInternal *map, HashMapAlgorithm algo,
                                 HashMapHashFn hash_fn, HashMapEqFn eq_fn,
                                 size_t key_size, size_t key_alignment,
                                 size_t value_size, size_t value_alignment);

#define hashmap_init_with_algo(map, algo, hash_fn, eq_fn)                      \
  hashmap_init_with_algo_impl(                                                 \
      (&(map)->internal), (algo), (hash_fn), (eq_fn), hashmap_key_size(map),   \
      hashmap_alignment_key(map), hashmap_value_size(map),                     \
      hashmap_alignment_value(map))

void hashmap_init_impl(HashMapInternal *map, HashMapHashFn hash_fn,
                       HashMapEqFn eq_fn, size_t key_size, size_t key_alignment,
                       size_t value_size, size_t value_alignment);

#define hashmap_init(map)                                                      \
  hashmap_init_impl((&(map)->internal),                                        \
                    (hashmap_hash_fn_for_key(hashmap_key(map))),               \
                    (hashmap_eq_fn_for_key(hashmap_key(map))),                 \
                    hashmap_key_size(map), hashmap_alignment_key(map),         \
                    hashmap_value_size(map), hashmap_alignment_value(map))

void hashmap_free_impl(HashMapInternal *map);

#define hashmap_free(map) hashmap_free_impl((&(map)->internal))

// Will try to insert the key into the map will return true if there already
// was a key previously inserted into the db
bool hashmap_put_impl(HashMapInternal *map, const void *key, const void *value);

#define hashmap_put(map, key, value)                                           \
  ({                                                                           \
    hashmap_key_type(map) _hashmap_key = (key);                                \
    hashmap_value_type(map) _hashmap_value = (value);                          \
    hashmap_put_impl(&(map)->internal, (const void *)&_hashmap_key,            \
                     (const void *)&_hashmap_value);                           \
  })

void *hashmap_get_impl(HashMapInternal *map, const void *key);

#define hashmap_get(map, key)                                                  \
  ({                                                                           \
    hashmap_key_type(map) _hashmap_key = (key);                                \
    (hashmap_value_ptr(map))                                                   \
        hashmap_get_impl(&(map)->internal, (const void *)&_hashmap_key);       \
  })

bool hashmap_remove_impl(HashMapInternal *map, const void *key);

#define hashmap_remove(map, key)                                               \
  ({                                                                           \
    hashmap_key_type(map) _hashmap_key = (key);                                \
    hashmap_remove_impl(&(map)->internal, (const void *)&_hashmap_key);        \
  })

size_t hashmap_len_impl(const HashMapInternal *map);

#define hashmap_len(map) hashmap_len_impl(&(map)->internal)

HashMapSlot hashmap_get_slot_impl(HashMapInternal *map, size_t idx);

#define hashmap_get_slot(map, idx)                                             \
  hashmap_get_slot_impl(&(map)->internal, (idx))

bool hashmap_is_empty_impl(HashMapInternal *map);

#define hashmap_is_empty(map) hashmap_is_empty_impl(&(map)->internal)

size_t hashmap_capacity_impl(HashMapInternal *map);

#define hashmap_capacity(map) hashmap_capacity_impl(&(map)->internal)

void hashmap_clear_impl(HashMapInternal *map);

#define hashmap_clear(map) hashmap_clear_impl(&(map)->internal)

bool hashmap_contains_impl(HashMapInternal *map, const void *key);

#define hashmap_contains(map, key)                                             \
  ({                                                                           \
    hashmap_key_type(map) _hashmap_key = (key);                                \
    hashmap_contains_impl(&(map)->internal, (const void *)&_hashmap_key);      \
  })

void hashmap_reserve_impl(HashMapInternal *map, size_t size);

#define hashmap_reserve(map, size) hashmap_reserve_impl(&(map)->internal, size)

void hashmap_set_load_factor_percent_impl(HashMapInternal *map,
                                          size_t load_factor_percent);

#define hashmap_set_load_factor_percent(map, load_factor_percent)              \
  hashmap_set_load_factor_percent_impl(&(map)->internal, load_factor_percent)

#ifdef HASHMAP_ENABLE_STATS
HashMapStats hashmap_stats_impl(const HashMapInternal *map);

#define hashmap_stats(map) hashmap_stats_impl(&(map)->internal)

void hashmap_stats_reset_impl(HashMapInternal *map);

#define hashmap_stats_reset(map) hashmap_stats_reset_impl(&(map)->internal)
#endif
#endif
