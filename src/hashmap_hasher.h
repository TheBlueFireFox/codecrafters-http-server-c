#ifndef HASHMAP_HASHER_H
#define HASHMAP_HASHER_H

#include "utils.h"
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* HASHING HELPER FUNCTIONS */
#define HASHMAP_INTEGER_TYPES(X)                                               \
  X(uint8_t, u8, 8)                                                            \
  X(uint16_t, u16, 16)                                                         \
  X(uint32_t, u32, 32)                                                         \
  X(uint64_t, u64, 64)

/* HASHING FUNCTIONS */
#define HASHMAP_HASH_CONTEXT_MAX_SIZE 64

typedef uint64_t Hash;

// VTable Alias for hash builder
typedef void (*HashMapContextInitAlgo)(void *config);
typedef void (*HashMapContextInit)(void *ctx, const void *config);
typedef void (*HashMapContextUpdate)(void *ctx, const void *data, size_t size);
typedef Hash (*HashMapContextFinalize)(void *ctx);

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                  \
  typedef void (*HashMapContextUpdate##suffix)(void *ctx, type value);

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                  \
  HashMapContextUpdate##suffix update_##suffix;

struct HashMapAlgorithm {
  // VTable for hash builder
  HashMapContextInitAlgo init_algorithm;
  HashMapContextInit init;

  HashMapContextUpdate update;
  HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)

  HashMapContextFinalize finalize;
};
typedef struct HashMapAlgorithm HashMapAlgorithm;

#undef HASHMAP_DEFINE_HASH_UPDATE

struct HashMapHashBuilder {
  ALIGNAS_MAX uint8_t ctx_data[HASHMAP_HASH_CONTEXT_MAX_SIZE];
  HashMapAlgorithm *algo;
};

typedef struct HashMapHashBuilder HashMapHashBuilder;

/* HASHING FNV1A FUNCTIONS */
struct Fnv1aContext {
  uint64_t state;
};

typedef struct Fnv1aContext Fnv1aContext;

void hashmap_fnv1a_init(void *ctx, const void *config);
void hashmap_fnv1a_update(void *ctx, const void *data, size_t size);

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                  \
  void hashmap_fnv1a_update_##suffix(void *ctx, type value);

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE
Hash hashmap_fnv1a_finalize(void *ctx);

extern const HashMapAlgorithm Fnv1a;

// Configuration based on run version
#define HASHMAP_HASH_SIPHON_MESSAGE 1
#define HASHMAP_HASH_SIPHON_FINAL 3

struct SiphashContext {
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;

  uint64_t total_len;

  uint8_t tail[8];
  size_t tail_len;
};

typedef struct SiphashContext SiphashContext;

struct SiphasConfig {
  uint64_t k0;
  uint64_t k1;
};

typedef struct SiphasConfig SiphasConfig;

void hashmap_siphash_init_algo(void *config);
void hashmap_siphash_init(void *ctx, const void *config);
void hashmap_siphash_update(void *ctx, const void *data, size_t size);
Hash hashmap_siphash_finalize(void *ctx);

extern const HashMapAlgorithm SipHash;

typedef void (*HashMapHashFn)(HashMapHashBuilder *builder, const void *key,
                              size_t key_size);

typedef bool (*HashMapEqFn)(const void *a, const void *b, size_t key_size);

/* HASH FUNCTIONS */
#define HASHMAP_FULL_INTEGER_TYPES(X)                                          \
  X(uint8_t, u8, 8)                                                            \
  X(uint16_t, u16, 16)                                                         \
  X(uint32_t, u32, 32)                                                         \
  X(uint64_t, u64, 64)                                                         \
  X(int8_t, i8, 8)                                                             \
  X(int16_t, i16, 16)                                                          \
  X(int32_t, i32, 32)                                                          \
  X(int64_t, i64, 64)

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                  \
  void hashmap_hash_##nice_suffix(HashMapHashBuilder *builder,                 \
                                  const void *key, size_t key_size);

HASHMAP_FULL_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE

void hashmap_hash_blob(HashMapHashBuilder *builder, const void *key,
                       size_t key_size);

void hashmap_hash_string(HashMapHashBuilder *builder, const void *key,
                         size_t key_size);

void hashmap_unsupported_key_type(void);

#define hashmap_hash_fn_for_key(key_expr)                                      \
  _Generic((key_expr),                                                         \
      uint8_t: &hashmap_hash_u8,                                               \
      uint16_t: &hashmap_hash_u16,                                             \
      uint32_t: &hashmap_hash_u32,                                             \
      uint64_t: &hashmap_hash_u64,                                             \
      int8_t: &hashmap_hash_i8,                                                \
      int16_t: &hashmap_hash_i16,                                              \
      int32_t: &hashmap_hash_i32,                                              \
      int64_t: &hashmap_hash_i64,                                              \
      char *: &hashmap_hash_string,                                            \
      const char *: &hashmap_hash_string,                                      \
      default: &hashmap_unsupported_key_type)

// cstr
bool hashmap_equal_string(const void *a, const void *b, size_t key_size);

// for int, uint64_t enum Foo etc...
bool hashmap_equal_bytes(const void *a, const void *b, size_t key_size);

#define hashmap_eq_fn_for_key(key_expr)                                        \
  _Generic((key_expr),                                                         \
      uint8_t: &hashmap_equal_bytes,                                           \
      uint16_t: &hashmap_equal_bytes,                                          \
      uint32_t: &hashmap_equal_bytes,                                          \
      uint64_t: &hashmap_equal_bytes,                                          \
      int8_t: &hashmap_equal_bytes,                                            \
      int16_t: &hashmap_equal_bytes,                                           \
      int32_t: &hashmap_equal_bytes,                                           \
      int64_t: &hashmap_equal_bytes,                                           \
      char *: &hashmap_equal_string,                                            \
      const char *: &hashmap_equal_string,                                      \
      default: &hashmap_unsupported_key_type)

#endif
