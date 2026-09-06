#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdalign.h>
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

struct HashMapSlotHeader {
  Hash hash;
  bool occupied;
};

typedef struct HashMapSlotHeader HashMapSlotHeader;

struct HashMapSlot {
  HashMapSlotHeader *header;
  void *key;
  void *value;
};

typedef struct HashMapSlot HashMapSlot;

struct HashMapInternal {
  size_t len;
  size_t capacity;
  HashMapAlgorithm hash_algo;
  HashMapHashFn hash_fn;
  HashMapEqFn eq_fn;
  size_t key_size;
  size_t key_offset;
  size_t value_size;
  size_t value_offset;
  size_t slot_size;
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
      hashmap_alignment_value(map));

void hashmap_init_impl(HashMapInternal *map, HashMapHashFn hash_fn,
                       HashMapEqFn eq_fn, size_t key_size, size_t key_alignment,
                       size_t value_size, size_t value_alignment);

#define hashmap_init(map, hash_fn, eq_fn)                                      \
  hashmap_init_impl((&(map)->internal), (hash_fn), (eq_fn),                    \
                    hashmap_key_size(map), hashmap_alignment_key(map),         \
                    hashmap_value_size(map), hashmap_alignment_value(map));

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

/* HASH FUNCTIONS */

// cstr
bool hashmap_equal_cstr(const void *a, const void *b, size_t key_size);

// for int, uint64_t enum Foo etc...
bool hashmap_equal_bytes(const void *a, const void *b, size_t key_size);

#endif
