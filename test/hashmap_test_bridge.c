#include "hashmap_test_bridge.h"

#define HASHMAP_TEST_DEFINE_INIT(name, key_type, value_type)                   \
  void hashmap_test_init_##name(name##HashMapTest *map) { hashmap_init(map); }

HASHMAP_TEST_TYPES(HASHMAP_TEST_DEFINE_INIT)

void hashmap_test_init_StringU64(StringU64HashMapTest *map) {
  hashmap_init_with_algo(map, Fnv1a, &hashmap_hash_string,
                         &hashmap_equal_string);
}

void hashmap_test_init_StringInt(StringIntHashMapTest *map) {
  hashmap_init_with_algo(map, Fnv1a, &hashmap_hash_string,
                         &hashmap_equal_string);
}

#undef HASHMAP_TEST_DEFINE_INIT
