/* hashmap_test_bridge.c */

#include "hashmap_test_bridge.h"

#define HASHMAP_TEST_DEFINE_INIT(name, key_type, value_type)                   \
  void hashmap_test_init_##name(name##HashMapTest *map) { hashmap_init(map); }

HASHMAP_TEST_TYPES(HASHMAP_TEST_DEFINE_INIT)

#undef HASHMAP_TEST_DEFINE_INIT
