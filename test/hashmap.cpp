#include <gmock/gmock.h>

extern "C" {
#include "hashmap.h"
}

struct CountingContext {
  uint64_t state;
};

namespace {
void hashmap_first_byte_init(void *ctx) {
  CountingContext *ictx = (CountingContext *)ctx;
  ictx->state = 0;
}

void hashmap_first_byte_update(void *ctx, const void *data, size_t size) {
  CountingContext *ictx = (CountingContext *)ctx;
  std::ignore = size;
  if (ictx->state == 0) {
    ictx->state = *(const uint8_t *)data;
  }
}

Hash hashmap_first_byte_finalize(void *ctx) {
  CountingContext *ictx = (CountingContext *)ctx;
  return ictx->state;
}

const HashMapAlgorithm FirstByteAlgo = {
    .init = &hashmap_first_byte_init,
    .update = &hashmap_first_byte_update,
    .finalize = &hashmap_first_byte_finalize,
};

Hash hashmap_calculate_hash(HashMapInternal *map, const void *key) {
  HashMapHashBuilder ctx = {
      .update = map->hash_algo.update,
  };
  map->hash_algo.init(&ctx.ctx);
  map->hash_fn(&ctx, key, map->key_size);
  return map->hash_algo.finalize(&ctx.ctx);
}

HashMapSlot get_slot(HashMapInternal *map, size_t idx) {
  size_t slot_size =
      sizeof(HashMapSlotHeader) + map->key_size + map->value_size;

  uint8_t *slot_location = map->data + (slot_size * idx);
  HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
  void *key = slot_location + sizeof(HashMapSlotHeader);
  void *value = slot_location + sizeof(HashMapSlotHeader) + map->key_size;
  return (HashMapSlot){
      .header = header,
      .key = key,
      .value = value,
  };
}

} // namespace

TEST(TestHashMap, firstByteAlgo) {
  HashMap(int, const char *) map;

  hashmap_init_with_algo(&map, FirstByteAlgo, &hashmap_hash_int,
                         &hashmap_equal_bytes);

  int key = 1;
  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  EXPECT_EQ(key, hash);

  hashmap_free(&map);
}

class TestHashersU8
    : public ::testing::TestWithParam<std::tuple<uint8_t, Hash>> {};

TEST_P(TestHashersU8, FNV1aHashU8) {
  auto [key, expectedResult] = GetParam();

  HashMap(uint8_t, const char *) map;

  hashmap_init_with_algo(&map, Fnv1a, &hashmap_hash_u8, &hashmap_equal_bytes);

  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  EXPECT_EQ(expectedResult, hash);

  hashmap_free(&map);
}

INSTANTIATE_TEST_SUITE_P(
    FNV1aHashTests, TestHashersU8,
    ::testing::Values(std::make_tuple(0x01, 0xaf63bc4c8601b62c),
                      std::make_tuple(0x10, 0xaf63cd4c8601d30f),
                      std::make_tuple(0x0A, 0xaf63c74c8601c8dd),
                      std::make_tuple(0xFA, 0xaf64774c8602f3ed),
                      std::make_tuple(0xFF, 0xaf64724c8602eb6e)));

class TestHashersU64le
    : public ::testing::TestWithParam<std::tuple<uint64_t, Hash>> {
protected:
  static void store_le64(uint8_t out[8], uint64_t x) {
    out[0] = (uint8_t)(x >> 0);
    out[1] = (uint8_t)(x >> 8);
    out[2] = (uint8_t)(x >> 16);
    out[3] = (uint8_t)(x >> 24);
    out[4] = (uint8_t)(x >> 32);
    out[5] = (uint8_t)(x >> 40);
    out[6] = (uint8_t)(x >> 48);
    out[7] = (uint8_t)(x >> 56);
  }
};

TEST_P(TestHashersU64le, FNV1aHashU64le) {
  auto [key, expectedResult] = GetParam();

  HashMap(uint64_t, const char *) map;

  hashmap_init_with_algo(&map, Fnv1a, &hashmap_hash_u64, &hashmap_equal_bytes);

  uint8_t key_le[64];
  store_le64(key_le, key);

  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  EXPECT_EQ(expectedResult, hash);

  hashmap_free(&map);
}

INSTANTIATE_TEST_SUITE_P(
    FNV1aHashTests, TestHashersU64le,
    ::testing::Values(std::make_tuple(0x0000000000000000, 0xa8c7f832281a39c5),
                      std::make_tuple(0x5555555555555555, 0xb69a164af2be14ad),
                      std::make_tuple(0x0102030405060708, 0x0c6d4496e17859d5)));

TEST(TestHashMap, FNV1a) {
  HashMap(uint8_t, const char *) map;

  hashmap_init_with_algo(&map, Fnv1a, &hashmap_hash_u8, &hashmap_equal_bytes);

  // 0x01 00 00 00
  uint8_t key = 1;
  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  Hash expected = 0xaf63bc4c8601b62c;
  EXPECT_EQ(hash, expected);

  hashmap_free(&map);
}

TEST(TestHashMap, initFNV1a) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  EXPECT_EQ(map.internal.key_size, sizeof(int));
  EXPECT_EQ(map.internal.value_size, sizeof(const char *));

  EXPECT_EQ(map.internal.hash_fn, &hashmap_hash_int);
  EXPECT_EQ(map.internal.eq_fn, &hashmap_equal_bytes);
  EXPECT_NE(map.internal.data, nullptr);

  EXPECT_EQ(map.internal.len, 0);
  EXPECT_EQ(map.internal.capacity, HASHMAP_DEFAULT_CAPACITY);

  size_t slot_size = sizeof(HashMapSlotHeader) + map.internal.key_size +
                     map.internal.value_size;

  size_t map_size = slot_size * HASHMAP_DEFAULT_CAPACITY;

  for (size_t i = 0; i < map_size; i += slot_size) {
    uint8_t *slot_location = map.internal.data + (slot_size * i);
    HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
    EXPECT_FALSE(header->occupied);
  }

  hashmap_free(&map);
}

TEST(TestHashMap, free) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  hashmap_free(&map);

  EXPECT_EQ(map.internal.hash_fn, nullptr);
  EXPECT_EQ(map.internal.eq_fn, nullptr);
  EXPECT_EQ(map.internal.data, nullptr);
}

TEST(TestHashMap, putFNV1a) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  int key = 42;
  const char *value = "hello";

  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  size_t idx = hash % map.internal.capacity;
  size_t slot_size =
      sizeof(HashMapSlotHeader) + sizeof(key) + sizeof(const char *);

  bool had_entry = hashmap_put(&map, key, value);
  auto len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_FALSE(had_entry);

  // as we only have a single value at the given slot,
  // we can just assume alot about the internal structure of the
  // hashmap

  uint8_t *slot_location = map.internal.data + (slot_size * idx);
  HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
  const int *saved_key = (int *)(slot_location + sizeof(HashMapSlotHeader));
  const char **saved_value =
      (const char **)(slot_location + sizeof(HashMapSlotHeader) +
                      map.internal.key_size);

  EXPECT_TRUE(header->occupied);
  EXPECT_EQ(header->hash, hash);
  EXPECT_EQ(*saved_key, key);
  EXPECT_STREQ(*saved_value, value);

  hashmap_free(&map);
}

TEST(TestHashMap, putFirstByteAlgo) {
  HashMap(int, const char *) map;

  hashmap_init_with_algo(&map, FirstByteAlgo, &hashmap_hash_int,
                         &hashmap_equal_bytes);

  int key = 1;
  const char *value = "hello";
  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  EXPECT_EQ(hash, key);

  bool had_entry = hashmap_put(&map, key, value);
  auto len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_FALSE(had_entry);

  // as we only have a single value at the given slot,
  // we can just assume alot about the internal structure of the
  // hashmap

  HashMapSlot slot = get_slot(&map.internal, key);

  EXPECT_TRUE(slot.header->occupied);
  EXPECT_EQ(slot.header->hash, hash);
  EXPECT_EQ(*(int *)slot.key, key);
  EXPECT_STREQ(*(const char **)slot.value, value);

  hashmap_free(&map);
}

TEST(TestHashMap, putOverride) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  int key = 42;
  const char *value = "hello";

  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  size_t idx = hash % map.internal.capacity;
  size_t slot_size =
      sizeof(HashMapSlotHeader) + sizeof(key) + sizeof(const char *);

  bool had_entry = hashmap_put(&map, key, value);
  auto len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_FALSE(had_entry);

  const char *value2 = "helloMaybe?";

  had_entry = hashmap_put(&map, key, value2);
  len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_TRUE(had_entry);

  // as we only have a single value at the given slot,
  // we can just assume alot about the internal structure of the
  // hashmap

  uint8_t *slot_location = map.internal.data + (slot_size * idx);
  HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
  const int *saved_key = (int *)(slot_location + sizeof(HashMapSlotHeader));
  const char **saved_value =
      (const char **)(slot_location + sizeof(HashMapSlotHeader) +
                      map.internal.key_size);

  EXPECT_TRUE(header->occupied);
  EXPECT_EQ(header->hash, hash);
  EXPECT_EQ(*saved_key, key);
  EXPECT_STREQ(*saved_value, value2);

  hashmap_free(&map);
}

TEST(TestHashMap, putMultiple) {
  HashMap(int, const char *) map;

  hashmap_init_with_algo(&map, FirstByteAlgo, &hashmap_hash_int,
                         &hashmap_equal_bytes);

  const int keys[] = {1, 17, 2, 33};
  const char *values[] = {"one", "seventeen", "two", "thirty-three"};

  for (size_t i = 0; i < 4; i += 1) {
    EXPECT_FALSE(hashmap_put(&map, keys[i], values[i]));
  }

  EXPECT_EQ(hashmap_len(&map), 4);

  const int expected_keys[] = {1, 17, 33, 2};
  const char *expected_values[] = {"one", "seventeen", "thirty-three", "two"};

  for (size_t i = 0; i < 4; i += 1) {
    HashMapSlot slot = get_slot(&map.internal, i + 1);

    ASSERT_TRUE(slot.header->occupied);
    EXPECT_EQ(slot.header->hash, expected_keys[i]);
    EXPECT_EQ(*(int *)slot.key, expected_keys[i]);
    EXPECT_STREQ(*(const char **)slot.value, expected_values[i]);
  }

  for (size_t i = 0; i < 4; i += 1) {
    const char **result = hashmap_get(&map, keys[i]);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(*result, values[i]);
  }

  hashmap_free(&map);
}

TEST(TestHashMap, putAndGet) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  int key = 42;
  const char *value = "hello";

  bool had_entry = hashmap_put(&map, key, value);
  auto len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_FALSE(had_entry);

  const char **result = hashmap_get(&map, key);

  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(*result, value);

  hashmap_free(&map);
}

TEST(TestHashMap, putAndOverrideAndGet) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  int key = 42;
  const char *value = "hello";

  bool had_entry = hashmap_put(&map, key, value);
  auto len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_FALSE(had_entry);

  const char *value2 = "helloMaybe?";

  had_entry = hashmap_put(&map, key, value2);
  len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_TRUE(had_entry);

  const char **result = hashmap_get(&map, key);

  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(*result, value2);

  hashmap_free(&map);
}

TEST(TestHashMap, stringKeys) {
  HashMap(const char *, int) map;

  hashmap_init(&map, &hashmap_hash_string, &hashmap_equal_cstr);

  const char *first_key = "first";
  const char *second_key = "second";
  int first_value_to_store = 10;
  int second_value = 20;
  EXPECT_FALSE(hashmap_put(&map, first_key, first_value_to_store));
  EXPECT_FALSE(hashmap_put(&map, second_key, second_value));
  EXPECT_EQ(hashmap_len(&map), 2);

  const char *equivalent_first_key = "first";
  int *first_value = hashmap_get(&map, equivalent_first_key);
  ASSERT_NE(first_value, nullptr);
  EXPECT_EQ(*first_value, 10);

  int replacement_value = 100;
  EXPECT_TRUE(hashmap_put(&map, equivalent_first_key, replacement_value));
  EXPECT_EQ(*hashmap_get(&map, first_key), 100);

  EXPECT_TRUE(hashmap_remove(&map, second_key));
  EXPECT_EQ(hashmap_get(&map, second_key), nullptr);
  EXPECT_EQ(hashmap_len(&map), 1);

  hashmap_free(&map);
}

TEST(TestHashMap, removeEmpty) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  int key = 42;

  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  size_t idx = hash % map.internal.capacity;

  EXPECT_EQ(map.internal.len, 0);
  bool removed = hashmap_remove(&map, key);
  EXPECT_FALSE(removed);
  EXPECT_EQ(map.internal.len, 0);

  size_t slot_size = sizeof(HashMapSlotHeader) + map.internal.key_size +
                     map.internal.value_size;
  uint8_t *slot_location = map.internal.data + (slot_size * idx);
  HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
  EXPECT_FALSE(header->occupied);

  hashmap_free(&map);
}

TEST(TestHashMap, removeSomething) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  int key = 42;
  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  size_t idx = hash % map.internal.capacity;
  const char *value = "hello";
  hashmap_put(&map, key, value);

  bool removed = hashmap_remove(&map, key);
  EXPECT_TRUE(removed);

  size_t slot_size = sizeof(HashMapSlotHeader) + map.internal.key_size +
                     map.internal.value_size;
  uint8_t *slot_location = map.internal.data + (slot_size * idx);
  HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
  EXPECT_FALSE(header->occupied);

  hashmap_free(&map);
}

TEST(TestHashMap, removeMultiple) {
  HashMap(int, const char *) map;

  hashmap_init_with_algo(&map, FirstByteAlgo, &hashmap_hash_int,
                         &hashmap_equal_bytes);

  const int keys[] = {1, 17, 2, 33};
  const char *values[] = {"one", "seventeen", "two", "thirty-three"};

  for (size_t i = 0; i < 4; i += 1) {
    EXPECT_FALSE(hashmap_put(&map, keys[i], values[i]));
  }

  const int removed_key = 17;
  EXPECT_TRUE(hashmap_remove(&map, removed_key));
  EXPECT_EQ(hashmap_len(&map), 3);

  const int expected_keys[] = {1, 33, 2};
  const char *expected_values[] = {"one", "thirty-three", "two"};

  for (size_t i = 0; i < 3; i += 1) {
    HashMapSlot slot = get_slot(&map.internal, i + 1);

    ASSERT_TRUE(slot.header->occupied);
    EXPECT_EQ(slot.header->hash, expected_keys[i]);
    EXPECT_EQ(*(int *)slot.key, expected_keys[i]);
    EXPECT_STREQ(*(const char **)slot.value, expected_values[i]);
  }

  EXPECT_FALSE(get_slot(&map.internal, 4).header->occupied);
  EXPECT_EQ(hashmap_get(&map, removed_key), nullptr);

  for (size_t i = 0; i < 4; i += 1) {
    if (keys[i] == 17) {
      continue;
    }

    const char **result = hashmap_get(&map, keys[i]);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(*result, values[i]);
  }

  hashmap_free(&map);
}

TEST(TestHashMap, removeWrapAroundCluster) {
  HashMap(int, const char *) map;

  hashmap_init_with_algo(&map, FirstByteAlgo, &hashmap_hash_int,
                         &hashmap_equal_bytes);

  const int keys[] = {15, 31, 47};
  const char *values[] = {"fifteen", "thirty-one", "forty-seven"};

  for (size_t i = 0; i < 3; i += 1) {
    EXPECT_FALSE(hashmap_put(&map, keys[i], values[i]));
  }

  EXPECT_TRUE(
      get_slot(&map.internal, map.internal.capacity - 1).header->occupied);
  EXPECT_TRUE(get_slot(&map.internal, 0).header->occupied);
  EXPECT_TRUE(get_slot(&map.internal, 1).header->occupied);

  EXPECT_TRUE(hashmap_remove(&map, keys[0]));
  EXPECT_EQ(hashmap_get(&map, keys[0]), nullptr);

  const char **result = hashmap_get(&map, keys[1]);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(*result, values[1]);
  result = hashmap_get(&map, keys[2]);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(*result, values[2]);

  EXPECT_TRUE(hashmap_remove(&map, keys[1]));
  EXPECT_EQ(hashmap_get(&map, keys[1]), nullptr);

  result = hashmap_get(&map, keys[2]);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(*result, values[2]);

  EXPECT_EQ(hashmap_len(&map), 1);
  hashmap_free(&map);
}

TEST(TestHashMap, fillToCapacity) {
  HashMap(int, int) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  const size_t initial_capacity = map.internal.capacity;
  // we resize after overrunning the load factor not before
  const size_t entries_before_resize =
      ((initial_capacity * HASHMAP_LOAD_FACTOR_PERCENT) / 100);

  for (int key = 0; key < entries_before_resize; key += 1) {
    int value = key * 10;
    EXPECT_FALSE(hashmap_put(&map, key, value));
  }

  EXPECT_EQ(hashmap_len(&map), entries_before_resize);
  EXPECT_EQ(map.internal.capacity, initial_capacity);

  for (int key = 0; key < entries_before_resize; key += 1) {
    int *value = hashmap_get(&map, key);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, (int)(key * 10));
  }

  hashmap_free(&map);
}

TEST(TestHashMap, resizePreservesEntries) {
  HashMap(int, int) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  const size_t initial_capacity = map.internal.capacity;

  // we resize after overrunning the load factor not before
  const size_t entries_before_resize =
      ((initial_capacity * HASHMAP_LOAD_FACTOR_PERCENT) / 100) + 1;

  for (int key = 0; key < entries_before_resize; key += 1) {
    int value = key + 100;
    EXPECT_FALSE(hashmap_put(&map, key, value));
  }

  EXPECT_EQ(map.internal.capacity, initial_capacity * 2);
  EXPECT_EQ(hashmap_len(&map), entries_before_resize);

  for (int key = 0; key < entries_before_resize; key += 1) {
    int *value = hashmap_get(&map, key);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, (int)(key + 100));
  }

  const int new_key = (int)entries_before_resize + 1;
  int value = 999;
  EXPECT_FALSE(hashmap_put(&map, new_key, value));
  EXPECT_EQ(*hashmap_get(&map, new_key), value);

  int key2 = 0;
  value = 1234;
  EXPECT_TRUE(hashmap_put(&map, key2, value));
  EXPECT_EQ(*hashmap_get(&map, key2), value);

  EXPECT_TRUE(hashmap_remove(&map, new_key));
  EXPECT_EQ(hashmap_get(&map, new_key), nullptr);
  EXPECT_EQ(hashmap_len(&map), entries_before_resize);

  hashmap_free(&map);
}

TEST(TestHashMap, removeStopsBeforeIdealEntry) {
  HashMap(int, const char *) map;

  hashmap_init_with_algo(
      &map,
      FirstByteAlgo,
      &hashmap_hash_int,
      &hashmap_equal_bytes
  );

  int key1 = 1;
  int key2 = 17;
  int key3 = 3;

  const char *value1 = "one";
  const char *value2 = "seventeen";
  const char *value3 = "three";

  hashmap_put(&map, key1, value1);
  hashmap_put(&map, key2, value2);
  hashmap_put(&map, key3, value3);

  ASSERT_TRUE(hashmap_remove(&map, key2));

  EXPECT_EQ(hashmap_get(&map, key2), nullptr);

  const char **result = hashmap_get(&map, key3);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(*result, value3);

  hashmap_free(&map);
}
