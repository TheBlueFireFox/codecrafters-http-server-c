#include "vector.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

void init_vector_impl(VectorInternal *vec) {
  vec->capacity = 0;
  vec->len = 0;
  vec->ptr = NULL;
}

void init_vector_with_capacity_impl(VectorInternal *vec, size_t capacity,
                                    size_t obj_size) {
  vec->capacity = capacity;
  vec->len = 0;
  vec->ptr = malloc(capacity * obj_size);
  ASSERT(vec->ptr != NULL);
}

void free_vector_impl(VectorInternal *vec) {
  if (vec->ptr != NULL) {
    free(vec->ptr);
  }
  vec->ptr = NULL;
  vec->capacity = 0;
  vec->len = 0;
}

size_t len_vector_impl(VectorInternal *vec) { return vec->len; }

static void realloc_vector(VectorInternal *vec, size_t obj_size) {
  if (vec->ptr == NULL) {
    // new list
    vec->capacity = VECTOR_DEFAULT_CAPACITY;
  } else if (vec->len == vec->capacity) {
    // capacity * 2
    vec->capacity *= 2;
  }

  void *ptr = realloc(vec->ptr, vec->capacity * obj_size);

  if (ptr != NULL) {
    vec->ptr = ptr;
  }

  ASSERT(vec->ptr != NULL);
}

void clear_vector_impl(VectorInternal *vec) { vec->len = 0; }

void push_vector_impl(VectorInternal *vec, uint8_t const *const elem,
                      size_t obj_size) {
  if (vec->capacity == vec->len) {
    realloc_vector(vec, obj_size);
  }

  memcpy(vec->ptr + (vec->len * obj_size), elem, obj_size);
  vec->len += 1;
}

bool get_vector_impl(VectorInternal *vec, size_t idx, uint8_t *const val,
                     size_t obj_size) {
  if (vec->len <= idx) {
    return false;
  }
  memcpy(val, vec->ptr + (idx * obj_size), obj_size);
  return true;
}

void sort_vector_impl(VectorInternal *vec, size_t obj_size, compFn comp_fn) {
  qsort(vec->ptr, vec->len, obj_size, comp_fn);
}

void *search_vector_impl(VectorInternal *vec, size_t obj_size, const void *key,
                         compFn comp_fn) {
  return bsearch(key, vec->ptr, vec->len, obj_size, comp_fn);
}
