#include <gtest/gtest.h>

extern "C" {
#include "vector2.h"
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
  ASSERT_EQ(vector.internal.len, 0);

  size_t exp = ~0;

  push_vector(&vector, exp);

  size_t *got = get_vector(&vector, 0);
  ASSERT_EQ(exp, *got);

  ASSERT_EQ(vector.internal.len, 1);
  ASSERT_EQ(vector.internal.capacity, 1);

  free_vector(&vector);
}

TEST(TestVector, get) {
  Vector(size_t) vector;
  init_vector_with_capacity(&vector, 1);
  size_t exp = ~0;

  size_t *got = get_vector(&vector, 0);

  push_vector(&vector, exp);

  ASSERT_EQ(vector.internal.capacity, 1);
  ASSERT_EQ(vector.internal.len, 1);

  got = get_vector(&vector, 0);

  free_vector(&vector);
}

TEST(TestVector, realloc) {
  Vector(size_t) vector;
  init_vector(&vector);

  size_t exp = ~0;
  push_vector(&vector, exp);

  size_t *got = get_vector(&vector, 0);
  ASSERT_EQ(exp, *got);

  ASSERT_EQ(vector.internal.len, 1);
  ASSERT_EQ(vector.internal.capacity, VECTOR_DEFAULT_CAPACITY);

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
    size_t *got = get_vector(&vector, i);
    ASSERT_EQ(*got, 10 - i);
  }

  auto sort = [](const void *l, const void *r) -> int {
    const size_t *a = (const size_t *)l;
    const size_t *b = (const size_t *)r;

    return *a - *b;
  };

  sort_vector(&vector, sort);

  for (size_t i = 0; i < 10; i += 1) {
    size_t *got = get_vector(&vector, i);
    ASSERT_EQ(*got, i + 1);
  }

  free_vector(&vector);
}
