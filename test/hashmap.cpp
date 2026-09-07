#include <gmock/gmock.h>
#include <random>

extern "C" {
#include "hashmap.h"
}

namespace {
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

} // namespace

struct AlignmentCase {
  size_t header_size;
  size_t header_align;
  size_t key_size;
  size_t key_align;
  size_t value_size;
  size_t value_align;
};

class HashMapAlignmentTest : public ::testing::TestWithParam<AlignmentCase> {};

TEST_P(HashMapAlignmentTest, ProducesCorrectlyAlignedSlots) {
  const auto p = GetParam();

  HashMapSlotQuery query{
      .header_size = p.header_size,
      .header_alignment = p.header_align,
      .key_size = p.key_size,
      .key_alignment = p.key_align,
      .value_size = p.value_size,
      .value_alignment = p.value_align,
  };
  auto config = hashmap_slot_config(&query);

  constexpr size_t capacity = 32;

  std::vector<uint8_t> memory(capacity * config.slot_size, 0);

  // check that the offsets are correctly aligned
  for (size_t i = 0; i < capacity; ++i) {
    auto *slot = memory.data() + (i * config.slot_size);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(slot) % p.header_align, 0);

    EXPECT_EQ(
        reinterpret_cast<uintptr_t>(slot + config.key_offset) % p.key_align, 0);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(slot + config.value_offset) %
                  p.value_align,
              0);
  }

  size_t slot_align = std::max({p.key_align, p.header_align, p.value_align});

  EXPECT_EQ(config.key_offset % p.key_align, 0);
  EXPECT_EQ(config.value_offset % p.value_align, 0);
  EXPECT_EQ(config.slot_size % slot_align, 0);
}

INSTANTIATE_TEST_SUITE_P(HashMapSlotLayouts, HashMapAlignmentTest,
                         ::testing::Values(AlignmentCase{8, 8, 1, 1, 8, 8},
                                           AlignmentCase{8, 8, 8, 8, 1, 1},
                                           AlignmentCase{8, 8, 3, 1, 8, 8},
                                           AlignmentCase{8, 8, 4, 4, 16, 16},
                                           AlignmentCase{4, 4, 2, 2, 8, 8},
                                           AlignmentCase{16, 8, 3, 1, 4, 4}));

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

TEST(TestHashMap, getSlotByIndex) {
  HashMap(int, const char *) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  const size_t index = 7;
  HashMapSlot slot = hashmap_get_slot(&map, index);
  uint8_t *slot_location = map.internal.data + (index * map.internal.slot_size);

  EXPECT_EQ(reinterpret_cast<uint8_t *>(slot.header), slot_location);
  EXPECT_EQ(static_cast<uint8_t *>(slot.key),
            slot_location + map.internal.key_offset);
  EXPECT_EQ(static_cast<uint8_t *>(slot.value),
            slot_location + map.internal.value_offset);
  EXPECT_FALSE(slot.header->occupied);

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

  bool had_entry = hashmap_put(&map, key, value);
  auto len = hashmap_len(&map);
  EXPECT_EQ(len, 1);
  EXPECT_FALSE(had_entry);

  // as we only have a single value at the given slot,
  // we can just assume alot about the internal structure of the
  // hashmap

  auto slot = hashmap_get_slot(&map, idx);
  HashMapSlotHeader *header = slot.header;
  const int *saved_key = (int *)slot.key;
  const char **saved_value = (const char **)slot.value;

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

  HashMapSlot slot = hashmap_get_slot(&map, key);

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

  Hash hash = hashmap_calculate_hash(&map.internal, &key);
  size_t idx = hash % map.internal.capacity;
  size_t slot_size =
      sizeof(HashMapSlotHeader) + sizeof(key) + sizeof(const char *);

  bool had_entry = hashmap_put(&map, key, "hello");
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
  HashMapSlot slot = hashmap_get_slot(&map, idx);

  HashMapSlotHeader *header = slot.header;
  const int *saved_key = (int *)slot.key;
  const char **saved_value = (const char **)slot.value;

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
    HashMapSlot slot = hashmap_get_slot(&map, i + 1);

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
    HashMapSlot slot = hashmap_get_slot(&map, i + 1);

    ASSERT_TRUE(slot.header->occupied);
    EXPECT_EQ(slot.header->hash, expected_keys[i]);
    EXPECT_EQ(*(int *)slot.key, expected_keys[i]);
    EXPECT_STREQ(*(const char **)slot.value, expected_values[i]);
  }

  EXPECT_FALSE(hashmap_get_slot(&map, 4).header->occupied);
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
      hashmap_get_slot(&map, map.internal.capacity - 1).header->occupied);
  EXPECT_TRUE(hashmap_get_slot(&map, 0).header->occupied);
  EXPECT_TRUE(hashmap_get_slot(&map, 1).header->occupied);

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

  hashmap_init_with_algo(&map, FirstByteAlgo, &hashmap_hash_int,
                         &hashmap_equal_bytes);

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

TEST(TestHashMap, ContainsReturnsWhetherKeyExists) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  int key = 42;
  int value = 123;

  EXPECT_FALSE(hashmap_contains(&map, key));

  hashmap_put(&map, key, value);

  EXPECT_TRUE(hashmap_contains(&map, key));

  hashmap_remove(&map, key);

  EXPECT_FALSE(hashmap_contains(&map, key));

  hashmap_free(&map);
}

TEST(TestHashMap, IsEmptyReflectsMapContents) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  EXPECT_TRUE(hashmap_is_empty(&map));

  int key = 42;
  int value = 123;
  hashmap_put(&map, key, value);

  EXPECT_FALSE(hashmap_is_empty(&map));

  hashmap_remove(&map, key);

  EXPECT_TRUE(hashmap_is_empty(&map));

  hashmap_free(&map);
}

TEST(TestHashMap, CapacityReportsCurrentCapacity) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  const size_t initial_capacity = hashmap_capacity(&map);

  EXPECT_GT(initial_capacity, 0);

  hashmap_free(&map);
}

TEST(TestHashMap, CapacityGrowsWhenMapResizes) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  const size_t initial_capacity = hashmap_capacity(&map);

  for (size_t i = 0; i <= initial_capacity; ++i) {
    int key = static_cast<int>(i);
    int value = static_cast<int>(i * 10);

    hashmap_put(&map, key, value);
  }

  EXPECT_GT(hashmap_capacity(&map), initial_capacity);

  hashmap_free(&map);
}

TEST(TestHashMap, ClearRemovesAllEntries) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  for (int i = 0; i < 10; ++i) {
    int value = i * 10;
    hashmap_put(&map, i, value);
  }

  ASSERT_FALSE(hashmap_is_empty(&map));

  hashmap_clear(&map);

  EXPECT_TRUE(hashmap_is_empty(&map));
  EXPECT_EQ(map.internal.len, 0);

  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(hashmap_contains(&map, i));
    EXPECT_EQ(hashmap_get(&map, i), nullptr);
  }

  hashmap_free(&map);
}

TEST(TestHashMap, ClearLeavesMapReusable) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  hashmap_put(&map, 1, 10);
  hashmap_put(&map, 2, 20);

  hashmap_clear(&map);

  hashmap_put(&map, 3, 30);

  int *value = hashmap_get(&map, 3);

  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 30);

  EXPECT_FALSE(hashmap_contains(&map, 1));
  EXPECT_FALSE(hashmap_contains(&map, 2));

  hashmap_free(&map);
}

TEST(TestHashMap, ClearPreservesCapacity) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  for (int i = 0; i < 100; ++i) {
    hashmap_put(&map, i, i);
  }

  const size_t capacity_before_clear = hashmap_capacity(&map);

  hashmap_clear(&map);

  EXPECT_EQ(hashmap_capacity(&map), capacity_before_clear);

  hashmap_free(&map);
}

TEST(TestHashMap, ReserveIncreasesCapacity) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  const size_t initial_capacity = hashmap_capacity(&map);

  hashmap_reserve(&map, initial_capacity * 4);

  EXPECT_GT(hashmap_capacity(&map), initial_capacity);

  hashmap_free(&map);
}

TEST(TestHashMap, ReservePreservesExistingEntries) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  for (int i = 0; i < 10; ++i) {
    int value = i * 100;
    hashmap_put(&map, i, value);
  }

  hashmap_reserve(&map, 1000);

  for (int i = 0; i < 10; ++i) {
    int *value = hashmap_get(&map, i);

    ASSERT_NE(value, nullptr) << "missing key=" << i;

    EXPECT_EQ(*value, i * 100);
  }

  EXPECT_EQ(map.internal.len, 10);

  hashmap_free(&map);
}

TEST(TestHashMap, ReserveDoesNotShrinkCapacity) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  hashmap_reserve(&map, 1000);

  const size_t large_capacity = hashmap_capacity(&map);

  hashmap_reserve(&map, 1);

  EXPECT_EQ(hashmap_capacity(&map), large_capacity);

  hashmap_free(&map);
}

TEST(TestHashMap, ReservePreventsResizeForRequestedEntries) {
  HashMap(int, int) map;
  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  constexpr size_t requested_entries = 100;

  hashmap_reserve(&map, requested_entries);

  const size_t reserved_capacity = hashmap_capacity(&map);

  for (size_t i = 0; i < requested_entries; ++i) {
    int key = static_cast<int>(i);
    int value = static_cast<int>(i);

    hashmap_put(&map, key, value);

    EXPECT_EQ(hashmap_capacity(&map), reserved_capacity)
        << "unexpected resize after inserting " << (i + 1) << " entries";
  }

  hashmap_free(&map);
}

// AI generated tests
namespace {
constexpr int kOperations = 1000;
constexpr int kKeyRange = 256;

using IntIntHashMap = HashMap(int, int);

void assert_map_matches_reference(
    IntIntHashMap *map, const std::unordered_map<int, int> &reference) {

  EXPECT_EQ(map->internal.len, reference.size());

  for (int key = 0; key < kKeyRange; ++key) {
    int lookup_key = key;

    int *actual = hashmap_get(map, lookup_key);
    auto expected = reference.find(key);

    if (expected == reference.end()) {
      EXPECT_EQ(actual, nullptr) << "key=" << key;
    } else {
      ASSERT_NE(actual, nullptr) << "key=" << key;
      EXPECT_EQ(*actual, expected->second) << "key=" << key;
    }
  }
}
} // namespace

class HashMapRandomizedTest : public ::testing::TestWithParam<std::uint32_t> {};

INSTANTIATE_TEST_SUITE_P(Seeds, HashMapRandomizedTest,
                         ::testing::Values(0U, 1U, 42U, 0xC0FFEEU, 0xDEADBEEFU,
                                           0xFFFFFFFFU));

TEST_P(HashMapRandomizedTest, MatchesStdUnorderedMap) {
  IntIntHashMap map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  std::unordered_map<int, int> reference;

  std::mt19937 rng(GetParam());

  std::uniform_int_distribution<int> operation_distribution(0, 2);
  std::uniform_int_distribution<int> key_distribution(0, kKeyRange - 1);
  std::uniform_int_distribution<int> value_distribution(-100000, 100000);

  for (int operation_index = 0; operation_index < kOperations;
       ++operation_index) {

    int operation = operation_distribution(rng);
    int key = key_distribution(rng);

    switch (operation) {
    case 0: {
      int value = value_distribution(rng);

      bool had_entry = hashmap_put(&map, key, value);

      bool reference_had_entry = reference.contains(key);

      EXPECT_EQ(had_entry, reference_had_entry)
          << "operation=" << operation_index << " key=" << key;

      reference[key] = value;
      break;
    }

    case 1: {
      bool removed = hashmap_remove(&map, key);

      bool reference_removed = reference.erase(key) != 0;

      EXPECT_EQ(removed, reference_removed)
          << "operation=" << operation_index << " key=" << key;

      break;
    }

    case 2: {
      int *actual = hashmap_get(&map, key);
      auto expected = reference.find(key);

      if (expected == reference.end()) {
        EXPECT_EQ(actual, nullptr)
            << "operation=" << operation_index << " key=" << key;
      } else {
        ASSERT_NE(actual, nullptr)
            << "operation=" << operation_index << " key=" << key;

        EXPECT_EQ(*actual, expected->second)
            << "operation=" << operation_index << " key=" << key;
      }

      break;
    }

    default:
      FAIL();
    }

    assert_map_matches_reference(&map, reference);
  }

  hashmap_free(&map);
}

TEST_P(HashMapRandomizedTest, CollisionHeavyMatchesReference) {
  HashMap(int, int) map;

  hashmap_init_with_algo(&map, FirstByteAlgo, &hashmap_hash_int,
                         &hashmap_equal_bytes);

  std::unordered_map<int, int> reference;

  std::mt19937 rng(GetParam());

  std::uniform_int_distribution<int> operation_distribution(0, 2);
  std::uniform_int_distribution<int> base_distribution(0, 15);
  std::uniform_int_distribution<int> collision_distribution(0, 31);
  std::uniform_int_distribution<int> value_distribution(-10000, 10000);

  for (int i = 0; i < kOperations; ++i) {
    int operation = operation_distribution(rng);

    /*
     * Lots of keys share the same low byte / bucket pattern,
     * depending on how FirstByteAlgo behaves.
     */
    int key = base_distribution(rng) + (collision_distribution(rng) * 256);

    switch (operation) {
    case 0: {
      int value = value_distribution(rng);

      bool expected_had_entry = reference.contains(key);

      bool actual_had_entry = hashmap_put(&map, key, value);

      EXPECT_EQ(actual_had_entry, expected_had_entry)
          << "iteration=" << i << " key=" << key;

      reference[key] = value;
      break;
    }

    case 1: {
      bool actual_removed = hashmap_remove(&map, key);

      bool expected_removed = reference.erase(key) != 0;

      EXPECT_EQ(actual_removed, expected_removed)
          << "iteration=" << i << " key=" << key;

      break;
    }

    case 2: {
      int *actual = hashmap_get(&map, key);
      auto expected = reference.find(key);

      if (expected == reference.end()) {
        EXPECT_EQ(actual, nullptr);
      } else {
        ASSERT_NE(actual, nullptr);
        EXPECT_EQ(*actual, expected->second);
      }

      break;
    }
    default:
      FAIL();
    }

    EXPECT_EQ(map.internal.len, reference.size());

    for (const auto &[expected_key, expected_value] : reference) {
      int lookup_key = expected_key;

      int *actual = hashmap_get(&map, lookup_key);

      ASSERT_NE(actual, nullptr)
          << "iteration=" << i << " key=" << expected_key;

      EXPECT_EQ(*actual, expected_value)
          << "iteration=" << i << " key=" << expected_key;
    }
  }

  hashmap_free(&map);
}

TEST_P(HashMapRandomizedTest, SurvivesRepeatedResizes) {
  HashMap(int, int) map;

  hashmap_init(&map, &hashmap_hash_int, &hashmap_equal_bytes);

  std::unordered_map<int, int> reference;

  std::mt19937 rng(GetParam());
  std::uniform_int_distribution<int> value_distribution(-1000000, 1000000);

  for (int key = 0; key < kOperations; ++key) {
    int value = value_distribution(rng);

    hashmap_put(&map, key, value);
    reference[key] = value;

    for (const auto &[expected_key, expected_value] : reference) {
      int lookup_key = expected_key;

      int *actual = hashmap_get(&map, lookup_key);

      ASSERT_NE(actual, nullptr)
          << "inserted_key=" << key << " missing_key=" << expected_key;

      EXPECT_EQ(*actual, expected_value);
    }

    EXPECT_EQ(map.internal.len, reference.size());
  }

  hashmap_free(&map);
}
} // namespace
