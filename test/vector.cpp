#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "vector.h"
}

TEST(TestVector, empty) {
  Vector(size_t) vector;
  init_vector(&vector);

  free_vector(&vector);
}

TEST(TestVector, withVector) {
  Vector(size_t) vector;
  init_vector_with_capacity(&vector, 1);

  free_vector(&vector);
}

TEST(TestVector, withCapacityPush) {
  Vector(size_t) vector;
  init_vector_with_capacity(&vector, 1);
  ASSERT_EQ(vector.internal.capacity, 1);
  ASSERT_EQ(len_vector(&vector), 0);

  size_t exp = ~0;

  push_vector(&vector, exp);

  size_t got;
  bool is_valid = get_vector(&vector, 0, &got);
  ASSERT_TRUE(is_valid);
  ASSERT_EQ(exp, got);

  ASSERT_EQ(vector.internal.len, 1);
  ASSERT_EQ(vector.internal.capacity, 1);

  free_vector(&vector);
}

TEST(TestVector, get) {
  Vector(size_t) vector;
  init_vector_with_capacity(&vector, 1);
  size_t exp = ~0;

  size_t got;
  bool is_valid = get_vector(&vector, 0, &got);
  ASSERT_FALSE(is_valid);

  push_vector(&vector, exp);

  ASSERT_EQ(vector.internal.capacity, 1);
  ASSERT_EQ(vector.internal.len, 1);

  is_valid = get_vector(&vector, 0, &got);
  ASSERT_TRUE(is_valid);

  free_vector(&vector);
}

TEST(TestVector, realloc) {
  Vector(size_t) vector;
  init_vector(&vector);

  size_t exp = ~0;
  push_vector(&vector, exp);

  size_t got;
  bool is_valid = get_vector(&vector, 0, &got);
  ASSERT_TRUE(is_valid);
  ASSERT_EQ(exp, got);

  ASSERT_EQ(vector.internal.len, 1);
  ASSERT_EQ(vector.internal.capacity, VECTOR_DEFAULT_CAPACITY);

  free_vector(&vector);
}

TEST(TestVector, foreach) {
  Vector(size_t) vector;
  init_vector_with_capacity(&vector, 10);

  std::vector<size_t> res;

  for (size_t i = 0; i < 10; i += 1) {
    size_t ii = 10 - ~i;
    res.push_back(ii);
    push_vector(&vector, ii);
  }

  std::vector<size_t> v;

  for (each(elem, &vector)) {
    v.push_back(*elem);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto l = res[i];
    auto r = v[i];
    ASSERT_EQ(l, r);
  }

  free_vector(&vector);
}

TEST(TestVector, foreachComplexType) {
  struct Foo {
    const char *key;
    const char *value;
  };
  Vector(Foo) vector;
  init_vector_with_capacity(&vector, 10);

  const char ABC[] = "ABCDEFGHIJKLMPNOQRSTUVWXYZ";

  std::vector<Foo> res;

  for (size_t i = 0; i < 10; i += 1) {
    Foo f = Foo(ABC + i, ABC + 10 + i);

    res.push_back(f);
    push_vector(&vector, f);
  }

  std::vector<Foo> get;

  for (size_t i = 0; i < 10; i += 1) {
    Foo c;
    bool is_valid = get_vector(&vector, i, &c);

    ASSERT_TRUE(is_valid);
    get.push_back(c);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto exp = res[i];
    auto got = get[i];
    EXPECT_STREQ(exp.key, got.key);
    EXPECT_STREQ(exp.value, got.value);
  }

  std::vector<Foo> feach;

  for (each(elem, &vector)) {
    feach.push_back(*elem);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto exp = res[i];
    auto got = feach[i];
    EXPECT_STREQ(exp.key, got.key);
    EXPECT_STREQ(exp.value, got.value);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto got_get = get[i];
    auto got_feach = feach[i];
    EXPECT_STREQ(got_get.key, got_feach.key);
    EXPECT_STREQ(got_get.value, got_feach.value);
  }

  free_vector(&vector);
}

TEST(TestVector, sorting) {
  Vector(size_t) vector;
  init_vector_with_capacity(&vector, 1);

  for (size_t i = 0; i < 10; i += 1) {
    size_t ii = 10 - i;
    push_vector(&vector, ii);
  }

  for (size_t i = 0; i < 10; i += 1) {
    size_t got;
    bool is_valid = get_vector(&vector, i, &got);
    ASSERT_TRUE(is_valid);
    ASSERT_EQ(got, 10 - i);
  }

  auto sort = [](const void *l, const void *r) -> int {
    const size_t *a = (const size_t *)l;
    const size_t *b = (const size_t *)r;

    return *a - *b;
  };

  sort_vector(&vector, sort);

  for (size_t i = 0; i < 10; i += 1) {
    size_t got;
    bool is_valid = get_vector(&vector, i, &got);
    ASSERT_TRUE(is_valid);
    ASSERT_EQ(got, i + 1);
  }

  free_vector(&vector);
}
