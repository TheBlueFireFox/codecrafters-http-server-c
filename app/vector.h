#ifndef VECTOR
#define VECTOR

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define START_SIZE 4

// Internal representation
// typeof(X)* ptr; (start of array)
// size_t len;
#define INIT_VECTOR(X)                                                         \
  struct Vector_##X {                                                          \
    X *ptr;                                                                    \
    size_t len;                                                                \
    size_t capacity;                                                           \
  };                                                                           \
                                                                               \
  typedef struct Vector_##X Vector_##X;                                        \
  __attribute__((unused)) static Vector_##X init_vector_##X() {                \
    Vector_##X v = {                                                           \
        .ptr = NULL,                                                           \
        .len = 0,                                                              \
        .capacity = 0,                                                         \
    };                                                                         \
    return v;                                                                  \
  }                                                                            \
                                                                               \
  __attribute__((unused)) static void free_vector_##X(Vector_##X *vec) {       \
    free(vec->ptr);                                                            \
    *vec = init_vector_##X();                                                  \
  }                                                                            \
                                                                               \
  __attribute__((unused)) static void realloc_vector_##X(Vector_##X *vec) {    \
    if (vec->ptr == NULL) {                                                    \
      /* new list */                                                           \
      vec->capacity = START_SIZE;                                              \
    } /* realloc */                                                            \
    else if (vec->len == vec->capacity) {                                      \
      /* capacity * 2 */                                                       \
      vec->capacity *= 2;                                                      \
    }                                                                          \
    vec->ptr = (X *)realloc(vec->ptr, sizeof(X) * vec->capacity);              \
    assert(vec->ptr != NULL);                                                  \
  }                                                                            \
                                                                               \
  __attribute__((unused)) static void push_vector_##X(Vector_##X *vec,         \
                                                      X elem) {                \
    if (vec->capacity == vec->len) {                                           \
      /* realloc */                                                            \
      realloc_vector_##X(vec);                                                 \
    }                                                                          \
    vec->ptr[vec->len] = elem;                                                 \
    vec->len += 1;                                                             \
  }

#endif // !VECTOR
