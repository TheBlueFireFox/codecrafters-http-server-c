#include "hashmap_hasher.h"
#include "utils.h"
#include <string.h>

const HashMapAlgorithm Fnv1a = {
    .init = &hashmap_fnv1a_init,
    .update = &hashmap_fnv1a_update,
    .finalize = &hashmap_fnv1a_finalize,
};

void hashmap_fnv1a_init(void *ctx) {
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

Hash hashmap_fnv1a_finalize(void *ctx) {
  Fnv1aContext *ctx_internal = ctx;
  return ctx_internal->state;
}

#define HASHMAP_DEFINE_HASH_UPDATE(type, suffix)                               \
  void hashmap_hash_##suffix(HashMapHashBuilder *builder, const void *key,     \
                             size_t key_size) {                                \
    ASSERT(key_size == sizeof(type));                                          \
    builder->update(builder->ctx.ctx_data, key, sizeof(type));                 \
  }

HASHMAP_INTEGER_TYPES(HASHMAP_DEFINE_HASH_UPDATE)

void hashmap_hash_blob(HashMapHashBuilder *builder, const void *key,
                       size_t key_size) {
  builder->update(builder->ctx.ctx_data, key, key_size);
  hashmap_hash_u64(builder, &key_size, sizeof(key_size));
}

void hashmap_hash_string(HashMapHashBuilder *builder, const void *key,
                         size_t key_size) {
  // key_size makes no sence in a string
  (void)key_size;
  const char *string = *(const char *const *)key;
  size_t size = strlen(string);
  builder->update(builder->ctx.ctx_data, string, size);
  hashmap_hash_u64(builder, &size, sizeof(size));
}
