#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "vector.h"
}

TEST(TestVector, empty) {
  Vector(size_t) vector;
  vector_init(&vector);

  vector_free(&vector);
}

TEST(TestVector, withVector) {
  Vector(size_t) vector;
  vector_init_with_capacity(&vector, 1);

  vector_free(&vector);
}

TEST(TestVector, withCapacityPush) {
  Vector(size_t) vector;
  vector_init_with_capacity(&vector, 1);
  ASSERT_EQ(vector.internal.capacity, 1);
  ASSERT_EQ(vector_len(&vector), 0);

  size_t exp = ~0;

  vector_push(&vector, exp);

  size_t got;
  bool is_valid = vector_get(&vector, 0, &got);
  ASSERT_TRUE(is_valid);
  ASSERT_EQ(exp, got);

  ASSERT_EQ(vector.internal.len, 1);
  ASSERT_EQ(vector.internal.capacity, 1);

  vector_free(&vector);
}

TEST(TestVector, get) {
  Vector(size_t) vector;
  vector_init_with_capacity(&vector, 1);
  size_t exp = ~0;

  size_t got;
  bool is_valid = vector_get(&vector, 0, &got);
  ASSERT_FALSE(is_valid);

  vector_push(&vector, exp);

  ASSERT_EQ(vector.internal.capacity, 1);
  ASSERT_EQ(vector.internal.len, 1);

  is_valid = vector_get(&vector, 0, &got);
  ASSERT_TRUE(is_valid);

  vector_free(&vector);
}

TEST(TestVector, clear) {
  Vector(size_t) vector;
  vector_init_with_capacity(&vector, 1);
  size_t exp = ~0;

  size_t got;
  bool is_valid = vector_get(&vector, 0, &got);
  ASSERT_FALSE(is_valid);

  vector_push(&vector, exp);

  ASSERT_EQ(vector.internal.capacity, 1);
  ASSERT_EQ(vector.internal.len, 1);

  vector_clear(&vector);

  ASSERT_EQ(vector.internal.capacity, 1);
  ASSERT_EQ(vector.internal.len, 0);

  vector_free(&vector);
}

TEST(TestVector, realloc) {
  Vector(size_t) vector;
  vector_init(&vector);

  size_t exp = ~0;
  vector_push(&vector, exp);

  size_t got;
  bool is_valid = vector_get(&vector, 0, &got);
  ASSERT_TRUE(is_valid);
  ASSERT_EQ(exp, got);

  ASSERT_EQ(vector.internal.len, 1);
  ASSERT_EQ(vector.internal.capacity, VECTOR_DEFAULT_CAPACITY);

  vector_free(&vector);
}

TEST(TestVector, forEach) {
  Vector(size_t) vector;
  vector_init_with_capacity(&vector, 10);

  std::vector<size_t> res;

  for (size_t i = 0; i < 10; i += 1) {
    size_t ii = 10 - ~i;
    res.push_back(ii);
    vector_push(&vector, ii);
  }

  std::vector<size_t> v;

  for (vector_each(elem, &vector)) {
    v.push_back(*elem);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto l = res[i];
    auto r = v[i];
    ASSERT_EQ(l, r);
  }

  vector_free(&vector);
}

TEST(TestVector, foreachComplexType) {
  struct Foo {
    const char *key;
    const char *value;
  };
  Vector(Foo) vector;
  vector_init_with_capacity(&vector, 10);

  const char ABC[] = "ABCDEFGHIJKLMPNOQRSTUVWXYZ";

  std::vector<Foo> res;

  for (size_t i = 0; i < 10; i += 1) {
    Foo f = Foo(ABC + i, ABC + 10 + i);

    res.push_back(f);
    vector_push(&vector, f);
  }

  std::vector<Foo> get;

  for (size_t i = 0; i < 10; i += 1) {
    Foo c;
    bool is_valid = vector_get(&vector, i, &c);

    ASSERT_TRUE(is_valid);
    get.push_back(c);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto exp = res[i];
    auto got = get[i];
    ASSERT_STREQ(exp.key, got.key);
    ASSERT_STREQ(exp.value, got.value);
  }

  std::vector<Foo> feach;

  for (vector_each(elem, &vector)) {
    feach.push_back(*elem);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto exp = res[i];
    auto got = feach[i];
    ASSERT_STREQ(exp.key, got.key);
    ASSERT_STREQ(exp.value, got.value);
  }

  for (size_t i = 0; i < 10; i += 1) {
    auto got_get = get[i];
    auto got_feach = feach[i];
    ASSERT_STREQ(got_get.key, got_feach.key);
    ASSERT_STREQ(got_get.value, got_feach.value);
  }

  vector_free(&vector);
}

TEST(TestVector, sorting) {
  Vector(size_t) vector;
  vector_init_with_capacity(&vector, 1);

  for (size_t i = 0; i < 10; i += 1) {
    size_t ii = 10 - i;
    vector_push(&vector, ii);
  }

  for (size_t i = 0; i < 10; i += 1) {
    size_t got;
    bool is_valid = vector_get(&vector, i, &got);
    ASSERT_TRUE(is_valid);
    ASSERT_EQ(got, 10 - i);
  }

  auto sort = [](const void *l, const void *r) -> int {
    const size_t *a = (const size_t *)l;
    const size_t *b = (const size_t *)r;

    return static_cast<int>(*a - *b);
  };

  vector_sort(&vector, sort);

  for (size_t i = 0; i < 10; i += 1) {
    size_t got;
    bool is_valid = vector_get(&vector, i, &got);
    ASSERT_TRUE(is_valid);
    ASSERT_EQ(got, i + 1);
  }

  vector_free(&vector);
}
