#pragma once

#include "hashmap.h"

/*
 * NAME              KEY TYPE        VALUE TYPE
 */
#define HASHMAP_TEST_TYPES(X)                                                  \
  X(U8U8, uint8_t, uint8_t)                                                    \
  X(U16U16, uint16_t, uint16_t)                                                \
  X(U32U32, uint32_t, uint32_t)                                                \
  X(U64U64, uint64_t, uint64_t)                                                \
  X(I8I8, int8_t, int8_t)                                                      \
  X(I16I16, int16_t, int16_t)                                                  \
  X(I32I32, int32_t, int32_t)                                                  \
  X(I64I64, int64_t, int64_t)                                                  \
  X(IntInt, int, int)                                                          \
  X(IntString, int, const char *)                                              \
  X(StringU64, const char *, uint64_t)                                         \
  X(StringInt, const char *, int)

/*
 * Generate named map types.
 */
#define HASHMAP_TEST_DECLARE_TYPE(name, key_type, value_type)                  \
  typedef HashMap(key_type, value_type) name##HashMapTest;

HASHMAP_TEST_TYPES(HASHMAP_TEST_DECLARE_TYPE)

#undef HASHMAP_TEST_DECLARE_TYPE

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generate bridge declarations.
 */
#define HASHMAP_TEST_DECLARE_INIT(name, key_type, value_type)                  \
  void hashmap_test_init_##name(name##HashMapTest *map);

HASHMAP_TEST_TYPES(HASHMAP_TEST_DECLARE_INIT)

#undef HASHMAP_TEST_DECLARE_INIT

#ifdef __cplusplus
}
#endif
