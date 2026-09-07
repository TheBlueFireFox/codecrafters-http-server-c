#ifndef HASHMAP_HASHER_H
#define HASHMAP_HASHER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* HASHING FUNCTIONS */
#define HASHMAP_HASH_CONTEXT_MAX_SIZE 64

typedef uint64_t Hash;

// VTable Alias for hash builder
typedef void (*HashMapContextInit)(void *ctx);
typedef void (*HashMapContextUpdate)(void *ctx, const void *data, size_t size);
typedef Hash (*HashMapContextFinalize)(void *ctx);

struct HashMapHashBuilder {
  HashMapContextUpdate update;
  union {
    max_align_t align;
    uint8_t ctx_data[HASHMAP_HASH_CONTEXT_MAX_SIZE];
  } ctx;
};

typedef struct HashMapHashBuilder HashMapHashBuilder;

struct HashMapAlgorithm {
  // VTable for hash builder
  HashMapContextInit init;
  HashMapContextUpdate update;
  HashMapContextFinalize finalize;
};

typedef struct HashMapAlgorithm HashMapAlgorithm;

/* HASHING FNV1A FUNCTIONS */
struct Fnv1aContext {
  uint64_t state;
};

typedef struct Fnv1aContext Fnv1aContext;

void hashmap_fnv1a_init(void *ctx);
void hashmap_fnv1a_update(void *ctx, const void *data, size_t size);
Hash hashmap_fnv1a_finalize(void *ctx);

extern const HashMapAlgorithm Fnv1a;

/* HASHING HELPER FUNCTIONS */
#define HASHMAP_INTEGER_TYPES(X)                                               \
  X(uint8_t, u8)                                                               \
  X(uint16_t, u16)                                                             \
  X(uint32_t, u32)                                                             \
  X(uint64_t, u64)                                                             \
  X(int8_t, i8)                                                                \
  X(int16_t, i16)                                                              \
  X(int32_t, i32)                                                              \
  X(int64_t, i64)                                                              \
  X(int, int)

typedef void (*HashMapHashFn)(HashMapHashBuilder *builder, const void *key,
                              size_t key_size);

typedef bool (*HashMapEqFn)(const void *a, const void *b, size_t key_size);

#define HASHMAP_DEFINE_HASH_UPDATE(type, suffix)                               \
  void hashmap_hash_##suffix(HashMapHashBuilder *builder, const void *key,     \
                             size_t key_size);

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE

void hashmap_hash_blob(HashMapHashBuilder *builder, const void *key,
                       size_t key_size);

void hashmap_hash_string(HashMapHashBuilder *builder, const void *key,
                         size_t key_size);
#endif
