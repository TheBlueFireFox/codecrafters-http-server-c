#include "hashmap_hasher.h"
#include "utils.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>

typedef uint8_t SipHashKey[16];

const HashMapAlgorithm SipHash = {
    .init_algorithm = &hashmap_siphash_init_algo,
    .init = &hashmap_siphash_init,
    .update = &hashmap_siphash_update,
    .finalize = &hashmap_siphash_finalize,
};

static int hashmap_siphash_key_generate(SipHashKey key) {
  unsigned char *p = (unsigned char *)key;
  size_t remaining = sizeof(SipHashKey);

  while (remaining > 0) {
    ssize_t n = getrandom(p, remaining, 0);

    if (n < 0) {
      if (errno == EINTR)
        continue;

      return -1;
    }

    p += n;
    remaining -= (size_t)n;
  }

  return 0;
}

static uint64_t load_u64_le(const uint8_t *bytes) {
  return ((uint64_t)bytes[0]) | ((uint64_t)bytes[1] << 8) |
         ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24) |
         ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) |
         ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);
}

void hashmap_siphash_init_algo(void *config) {
  SiphasConfig *key = config;
  SipHashKey key_raw;
  int result = hashmap_siphash_key_generate(key_raw);
  (void)result;
  ASSERT(result == 0);
  key->k0 = load_u64_le(key_raw);
  key->k1 = load_u64_le(key_raw + 8);
}

void hashmap_siphash_init(void *ctx, const void *config) {
  SiphashContext *hash = ctx;
  const SiphasConfig *key = config;

  hash->v0 = UINT64_C(0x736f6d6570736575) ^ key->k0;

  hash->v1 = UINT64_C(0x646f72616e646f6d) ^ key->k1;

  hash->v2 = UINT64_C(0x6c7967656e657261) ^ key->k0;

  hash->v3 = UINT64_C(0x7465646279746573) ^ key->k1;

  hash->total_len = 0;
  hash->tail_len = 0;
}

static uint64_t rotl64(uint64_t value, unsigned int bits) {
  return (value << bits) | (value >> (64 - bits));
}

static void sip_round(SiphashContext *hash) {
  hash->v0 += hash->v1;
  hash->v1 = rotl64(hash->v1, 13);
  hash->v1 ^= hash->v0;
  hash->v0 = rotl64(hash->v0, 32);

  hash->v2 += hash->v3;
  hash->v3 = rotl64(hash->v3, 16);
  hash->v3 ^= hash->v2;

  hash->v0 += hash->v3;
  hash->v3 = rotl64(hash->v3, 21);
  hash->v3 ^= hash->v0;

  hash->v2 += hash->v1;
  hash->v1 = rotl64(hash->v1, 17);
  hash->v1 ^= hash->v2;
  hash->v2 = rotl64(hash->v2, 32);
}

static void hashmap_hash_compress(SiphashContext *hash, uint64_t message) {
  hash->v3 ^= message;

  for (size_t i = 0; i < HASHMAP_HASH_SIPHON_MESSAGE; i += 1) {
    sip_round(hash);
  }

  hash->v0 ^= message;
}

void hashmap_siphash_update(void *ctx, const void *data, size_t size) {

  SiphashContext *hash = ctx;
  const uint8_t *bytes = data;

  hash->total_len += size;

  /*
   * Finish an existing partial block first.
   */
  if (hash->tail_len != 0) {
    size_t needed = 8 - hash->tail_len;

    if (needed > size)
      needed = size;

    memcpy(hash->tail + hash->tail_len, bytes, needed);

    hash->tail_len += needed;
    bytes += needed;
    size -= needed;

    if (hash->tail_len == 8) {
      hashmap_hash_compress(hash, load_u64_le(hash->tail));

      hash->tail_len = 0;
    }
  }

  /*
   * Process complete 8-byte blocks.
   */
  while (size >= 8) {
    hashmap_hash_compress(hash, load_u64_le(bytes));

    bytes += 8;
    size -= 8;
  }

  /*
   * Save whatever is left.
   */
  if (size != 0) {
    memcpy(hash->tail, bytes, size);
    hash->tail_len = size;
  }
}

Hash hashmap_siphash_finalize(void *ctx) {
  SiphashContext *hash = ctx;
  uint64_t final_block = (hash->total_len & UINT64_C(0xff)) << 56;

  for (size_t i = 0; i < hash->tail_len; ++i) {
    final_block |= (uint64_t)hash->tail[i] << (i * 8);
  }

  hash->v3 ^= final_block;

  for (size_t i = 0; i < HASHMAP_HASH_SIPHON_MESSAGE; i += 1) {
    sip_round(hash);
  }

  hash->v0 ^= final_block;

  hash->v2 ^= UINT64_C(0xff);

  for (size_t i = 0; i < HASHMAP_HASH_SIPHON_FINAL; i += 1) {
    sip_round(hash);
  }

  return hash->v0 ^ hash->v1 ^ hash->v2 ^ hash->v3;
}

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                  \
  .update_##suffix = &hashmap_fnv1a_update_##suffix,

const HashMapAlgorithm Fnv1a = {
    .init_algorithm = NULL,
    .init = &hashmap_fnv1a_init,
    .update = &hashmap_fnv1a_update,
    .finalize = &hashmap_fnv1a_finalize,
    HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)};

#undef HASHMAP_DEFINE_HASH_UPDATE

void hashmap_fnv1a_init(void *ctx, const void *config) {
  (void)config;
  Fnv1aContext *ctx_internal = ctx;
  ctx_internal->state = 0xcbf29ce484222325;
}

void hashmap_fnv1a_update(void *ctx, const void *data, size_t size) {
  Fnv1aContext *ctx_internal = ctx;
  Hash hash = ctx_internal->state;

  for (size_t i = 0; i < size; i += 1) {
    hash ^= *(((const uint8_t *)data) + i);
    hash *= 0x00000100000001b3;
  }
  ctx_internal->state = hash;
}

#define HASHMAP_DEFINE_FNV1A_UPDATE(type, nice_suffix, suffix)                 \
  void hashmap_fnv1a_update_##suffix(void *ctx, type value) {                  \
    Fnv1aContext *ctx_internal = ctx;                                          \
    const uint8_t *bytes = (const uint8_t *)&value;                            \
                                                                               \
    Hash state = ctx_internal->state;                                          \
    for (size_t i = 0; i < sizeof(type); ++i) {                                \
      state ^= bytes[i];                                                       \
      state *= 0x00000100000001b3;                                             \
    }                                                                          \
    ctx_internal->state = state;                                               \
  }

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_FNV1A_UPDATE)
#undef HASHMAP_DEFINE_FNV1A_UPDATE

Hash hashmap_fnv1a_finalize(void *ctx) {
  Fnv1aContext *ctx_internal = ctx;
  return ctx_internal->state;
}

#define HASHMAP_DEFINE_HASH_UPDATE(type, nice_suffix, suffix)                  \
  void hashmap_hash_##nice_suffix(HashMapHashBuilder *builder,                 \
                                  const void *key, size_t key_size) {          \
    ASSERT(key_size == sizeof(type));                                          \
    type value;                                                                \
    memcpy(&value, key, sizeof(type));                                         \
    if (builder->algo->update_##suffix != NULL) {                              \
      builder->algo->update_##suffix(builder->ctx_data, value);                \
    } else {                                                                   \
      builder->algo->update(builder->ctx_data, &value, sizeof(value));         \
    }                                                                          \
  }

HASHMAP_FULL_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)

#undef HASHMAP_DEFINE_HASH_UPDATE

void hashmap_hash_blob(HashMapHashBuilder *builder, const void *key,
                       size_t key_size) {
  builder->algo->update(builder->ctx_data, key, key_size);
  hashmap_hash_u64(builder, &key_size, sizeof(key_size));
}

void hashmap_hash_string(HashMapHashBuilder *builder, const void *key,
                         size_t key_size) {
  // key_size makes no sence in a string
  (void)key_size;
  const char *string = *(const char *const *)key;
  size_t size = strlen(string);
  builder->algo->update(builder->ctx_data, string, size);
  hashmap_hash_u64(builder, &size, sizeof(size));
}

bool hashmap_equal_string(const void *a, const void *b, size_t key_size) {
  (void)key_size;

  const char *sa = *(const char *const *)a;

  const char *sb = *(const char *const *)b;

  return strcmp(sa, sb) == 0;
}

bool hashmap_equal_bytes(const void *a, const void *b, size_t key_size) {
  return memcmp(a, b, key_size) == 0;
}
