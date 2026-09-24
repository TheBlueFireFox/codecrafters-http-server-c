#ifndef HASHMAP_HASHER_H
#define HASHMAP_HASHER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* HASHING HELPER FUNCTIONS */
#define HASHMAP_INTEGER_TYPES(X)                                                                   \
    X(uint8_t, u8, 8)                                                                              \
    X(uint16_t, u16, 16)                                                                           \
    X(uint32_t, u32, 32)                                                                           \
    X(uint64_t, u64, 64)

/* HASHING FUNCTIONS */
#define HASHMAP_HASH_CONTEXT_MAX_SIZE 64

typedef uint64_t Hash;

// VTable Alias for hash builder
typedef void (*HashMapContextInitAlgo)(void* config);
typedef void (*HashMapContextInit)(void* ctx, const void* config);
typedef void (*HashMapContextUpdate)(void* ctx, const void* data, size_t size);
typedef Hash (*HashMapContextFinalize)(void* ctx);

typedef Hash (*HashMapContextOneShot)(void* config, const void* data, size_t size);

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                                      \
    typedef void (*HashMapContextUpdate##suffix)(void* ctx, type value);

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                                      \
    HashMapContextUpdate##suffix update_##suffix;

struct HashMapAlgorithm {
    // VTable for hash builder
    size_t config_size;
    size_t context_size;
    HashMapContextInitAlgo init_algorithm;
    HashMapContextInit init;
    HashMapContextUpdate update;
    HashMapContextFinalize finalize;
    HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
};

typedef struct HashMapAlgorithm HashMapAlgorithm;

#undef HASHMAP_DEFINE_HASH_UPDATE

struct HashMapHashBuilder {
    uint8_t* ctx_data;
    HashMapAlgorithm* algo;
};

typedef struct HashMapHashBuilder HashMapHashBuilder;

/* HASHING FNV1A FUNCTIONS */
struct Fnv1aContext {
    uint64_t state;
};
#define HASHMAP_FNV1A_INITIAL_STATE UINT64_C(0xcbf29ce484222325)
typedef struct Fnv1aContext Fnv1aContext;

void hashmap_fnv1a_init(void* ctx, const void* config);
void hashmap_fnv1a_update(void* ctx, const void* data, size_t size);
Hash hashmap_fnv1a_finalize(void* ctx);

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                                      \
    void hashmap_fnv1a_update_##suffix(void* ctx, type value);

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE

#define HASHMAP_DEFINE_HASH_ONE_SHOT(type, nice_suffix, suffix)                                    \
    Hash hashmap_fnv1a_one_shot_##suffix(void* config, const void* key, size_t key_size);

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_ONE_SHOT)

#undef HASHMAP_DEFINE_HASH_UPDATE

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

void hashmap_siphash_init_algo(void* config);
void hashmap_siphash_init(void* ctx, const void* config);
void hashmap_siphash_update(void* ctx, const void* data, size_t size);
Hash hashmap_siphash_finalize(void* ctx);

extern const HashMapAlgorithm SipHash;

typedef void (*HashMapHashFn)(HashMapHashBuilder* builder, const void* key, size_t key_size);

typedef bool (*HashMapEqFn)(const void* a, const void* b, size_t key_size);

/* HASHMAP STRUCT */
struct HashMapHasher {
    HashMapAlgorithm algo;
    uint8_t* algo_config;
    uint8_t* algo_context;
    HashMapContextOneShot one_shot;
    HashMapHashFn hash_fn;
};

typedef struct HashMapHasher HashMapHasher;

/* HASH FUNCTIONS */
#define HASHMAP_FULL_INTEGER_TYPES(X, ...)                                                         \
    X(uint8_t, u8, 8 __VA_OPT__(, ) __VA_ARGS__)                                                   \
    X(uint16_t, u16, 16 __VA_OPT__(, ) __VA_ARGS__)                                                \
    X(uint32_t, u32, 32 __VA_OPT__(, ) __VA_ARGS__)                                                \
    X(uint64_t, u64, 64 __VA_OPT__(, ) __VA_ARGS__)                                                \
    X(int8_t, i8, 8 __VA_OPT__(, ) __VA_ARGS__)                                                    \
    X(int16_t, i16, 16 __VA_OPT__(, ) __VA_ARGS__)                                                 \
    X(int32_t, i32, 32 __VA_OPT__(, ) __VA_ARGS__)                                                 \
    X(int64_t, i64, 64 __VA_OPT__(, ) __VA_ARGS__)

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                                      \
    void hashmap_hash_##nice_suffix(HashMapHashBuilder* builder, const void* key, size_t key_size);

HASHMAP_FULL_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE

#define CAT_INNER(a, b) a##b
#define CAT(a, b) CAT_INNER(a, b)

#define HASHMAP_ONESHOT_CASE(type, suffix, bits, algo)                                             \
    type:                                                                                          \
    &CAT(CAT(CAT(hashmap_, algo), _one_shot_), bits),

#define HASHMAP_ONE_SHOT_FOR(type, algo)                                                           \
    _Generic(                                                                                      \
        (type),                                                                                    \
        HASHMAP_FULL_INTEGER_TYPES(                                                                \
            HASHMAP_ONESHOT_CASE,                                                                  \
            algo                                                                                   \
        ) default: &hashmap_unsupported_key_type                                                   \
    )

#define HASHMAP_HASHER_FOR(type, algo_name, const_algo)                                            \
    ((HashMapHasher){                                                                              \
        .algo = (const_algo),                                                                      \
        .one_shot = HASHMAP_ONE_SHOT_FOR(type, algo_name),                                         \
        .algo_config = NULL,                                                                       \
        .algo_context = NULL,                                                                      \
        .hash_fn = NULL,                                                                           \
    })

void hashmap_hash_blob(HashMapHashBuilder* builder, const void* key, size_t key_size);

void hashmap_hash_string(HashMapHashBuilder* builder, const void* key, size_t key_size);

void hashmap_unsupported_key_type(void);

#define hashmap_hash_fn_for_key(key_expr)                                                          \
    _Generic(                                                                                      \
        (key_expr),                                                                                \
        uint8_t: &hashmap_hash_u8,                                                                 \
        uint16_t: &hashmap_hash_u16,                                                               \
        uint32_t: &hashmap_hash_u32,                                                               \
        uint64_t: &hashmap_hash_u64,                                                               \
        int8_t: &hashmap_hash_i8,                                                                  \
        int16_t: &hashmap_hash_i16,                                                                \
        int32_t: &hashmap_hash_i32,                                                                \
        int64_t: &hashmap_hash_i64,                                                                \
        default: &hashmap_unsupported_key_type                                                     \
    )

// cstr
bool hashmap_equal_string(const void* a, const void* b, size_t key_size);

// for int, uint64_t enum Foo etc...
bool hashmap_equal_bytes(const void* a, const void* b, size_t key_size);

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                                      \
    bool hashmap_equal_##nice_suffix(const void* a, const void* b, size_t key_size);

HASHMAP_FULL_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)
#undef HASHMAP_DEFINE_HASH_UPDATE

#define hashmap_eq_fn_for_key(key_expr)                                                            \
    _Generic(                                                                                      \
        (key_expr),                                                                                \
        uint8_t: &hashmap_equal_u8,                                                                \
        uint16_t: &hashmap_equal_u16,                                                              \
        uint32_t: &hashmap_equal_u32,                                                              \
        uint64_t: &hashmap_equal_u64,                                                              \
        int8_t: &hashmap_equal_i8,                                                                 \
        int16_t: &hashmap_equal_i16,                                                               \
        int32_t: &hashmap_equal_i32,                                                               \
        int64_t: &hashmap_equal_i64,                                                               \
        default: &hashmap_unsupported_key_type                                                     \
    )

#endif
