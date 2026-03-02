#include "vector.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

void vector_init_impl(VectorInternal *vec) {
  vec->capacity = 0;
  vec->len = 0;
  vec->ptr = NULL;
}

void vector_init_with_capacity_impl(VectorInternal *vec, size_t capacity,
                                    size_t obj_size) {
  vec->capacity = capacity;
  vec->len = 0;
  vec->ptr = malloc(capacity * obj_size);
  ASSERT(vec->ptr != NULL);
}

void vector_free_impl(VectorInternal *vec) {
  if (vec->ptr != NULL) {
    free(vec->ptr);
  }
  vec->ptr = NULL;
  vec->capacity = 0;
  vec->len = 0;
}

size_t vector_len_impl(VectorInternal *vec) { return vec->len; }

static void vector_realloc(VectorInternal *vec, size_t obj_size) {
  size_t capacity = VECTOR_DEFAULT_CAPACITY;

  if (vec->ptr != NULL) {
    capacity = vec->capacity;
  }

  if (vec->ptr != NULL && vec->len == vec->capacity) {
    // capacity * 2
    capacity = vec->capacity * 2;
  }

  void *ptr = realloc(vec->ptr, capacity * obj_size);

  if (ptr != NULL) {
    vec->ptr = ptr;
    vec->capacity = capacity;
  }

  ASSERT(vec->ptr != NULL);
}

void vector_clear_impl(VectorInternal *vec) { vec->len = 0; }

void vector_push_impl(VectorInternal *vec, void const *const elem,
                      size_t obj_size) {

  if (vec->capacity == vec->len) {
    vector_realloc(vec, obj_size);
  }

  memcpy(vec->ptr + (vec->len * obj_size), elem, obj_size);
  vec->len += 1;
}

bool vector_get_impl(VectorInternal *vec, size_t idx, uint8_t *const val,
                     size_t obj_size) {
  if (vec->len <= idx) {
    return false;
  }
  memcpy(val, vec->ptr + (idx * obj_size), obj_size);
  return true;
}

void vector_sort_impl(VectorInternal *vec, size_t obj_size, compFn comp_fn) {
  qsort(vec->ptr, vec->len, obj_size, comp_fn);
}

void *vector_search_impl(VectorInternal *vec, size_t obj_size, const void *key,
                         compFn comp_fn) {
  return bsearch(key, vec->ptr, vec->len, obj_size, comp_fn);
}
