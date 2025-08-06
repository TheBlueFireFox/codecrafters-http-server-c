#include "vector.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

void _init_vector(VectorInternal *qi) {
  qi->capacity = 0;
  qi->len = 0;
  qi->ptr = NULL;
}

void _init_vector_with_capacity(VectorInternal *qi, size_t capacity,
                                size_t obj_size) {
  qi->capacity = capacity;
  qi->len = 0;
  qi->ptr = malloc(capacity * obj_size);
  ASSERT(qi->ptr != NULL);
}

void _free_vector(VectorInternal *qi) {
  if (qi->ptr != NULL) {
    free(qi->ptr);
  }
  qi->ptr = NULL;
  qi->capacity = 0;
  qi->len = 0;
}

size_t _len_vector(VectorInternal *vec) { return vec->len; }

static void _realloc_vector(VectorInternal *vec, size_t obj_size) {
  if (vec->ptr == NULL) {
    // new list
    vec->capacity = VECTOR_DEFAULT_CAPACITY;
  } else if (vec->len == vec->capacity) {
    // capacity * 2
    vec->capacity *= 2;
  }

  vec->ptr = realloc(vec->ptr, vec->capacity * obj_size);
  ASSERT(vec->ptr != NULL);
}

void _clear_vector(VectorInternal *vec) { vec->len = 0; }

void _push_vector(VectorInternal *vec, uint8_t const *const elem,
                  size_t obj_size) {
  if (vec->capacity == vec->len) {
    _realloc_vector(vec, obj_size);
  }

  memcpy(vec->ptr + (vec->len * obj_size), elem, obj_size);
  vec->len += 1;
}

bool _get_vector(VectorInternal *vec, size_t idx, uint8_t *val,
                 size_t obj_size) {
  if (vec->len <= idx) {
    return false;
  }
  memcpy(val, vec->ptr + idx * obj_size, obj_size);
  return true;
}

void _sort_vector(VectorInternal *vec, size_t obj_size, compFn comp_fn) {
  qsort(vec->ptr, vec->len, obj_size, comp_fn);
}

void *_search_vector(VectorInternal *vec, size_t obj_size, const void *key,
                     compFn comp_fn) {
  return bsearch(key, vec->ptr, vec->len, obj_size, comp_fn);
}
