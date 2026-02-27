#ifndef VECTOR_H
#define VECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VECTOR_DEFAULT_CAPACITY 4

struct VectorInternal {
  uint8_t *ptr;
  size_t len;
  size_t capacity;
};

typedef struct VectorInternal VectorInternal;

#define Vector(type)                                                           \
  union {                                                                      \
    VectorInternal internal;                                                   \
    /* NOLINTNEXTLINE bugprone-macro-parentheses */                            \
    type *payload;                                                             \
  }

#define payload_size(obj) sizeof(*(obj)->payload)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions prepares the vector and does not allocate
void init_vector_impl(VectorInternal *vec);

#define init_vector(vector) init_vector_impl((&(vector)->internal));

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions initialized the vector and preallocated capacity for
// the vector
void init_vector_with_capacity_impl(VectorInternal *vec, size_t capacity,
                                    size_t obj_size);

#define init_vector_with_capacity(vector, capacity)                            \
  init_vector_with_capacity_impl((&(vector)->internal), capacity,              \
                                 payload_size(vector));

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions frees the vector and resets everything to zero
void free_vector_impl(VectorInternal *vec);

#define free_vector(vector) free_vector_impl(&(vector)->internal)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions return the vectors len
size_t len_vector_impl(VectorInternal *vec);

#define len_vector(vector) len_vector_impl(&(vector)->internal)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions resets the vector size
void clear_vector_impl(VectorInternal *vec);

#define clear_vector(vector) clear_vector_impl(&(vector)->internal)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions adds an element to the vector
void push_vector_impl(VectorInternal *vec, uint8_t const *elem,
                      size_t obj_size);

#define push_vector(vector, item)                                              \
  push_vector_impl((&(vector)->internal),                                      \
                   (uint8_t const *const)(1 ? &(item) : ((vector)->payload)),  \
                   payload_size(vector))

bool get_vector_impl(VectorInternal *vec, size_t idx, uint8_t *val,
                     size_t obj_size);

#define get_vector(vector, idx, item)                                          \
  get_vector_impl(&(vector)->internal, idx,                                    \
                  (uint8_t *const)(1 ? (item) : ((vector)->payload)),          \
                  payload_size(vector))

typedef int (*compFn)(const void *param1, const void *param2);

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions will sort the whole vector inplace using the qsort function
void sort_vector_impl(VectorInternal *vec, size_t obj_size, compFn comp_fn);

#define sort_vector(vector, comp_fn)                                           \
  sort_vector_impl((&(vector)->internal), payload_size(vector), comp_fn)

void *search_vector_impl(VectorInternal *vec, size_t obj_size, const void *key,
                         compFn comp_fn);

#define search_vector(vector, key, comp_fn)                                    \
  search_vector_impl((&(vector)->internal), payload_size(vector),              \
                     (const void *)(key), (comp_fn))

// C23 extention typeof
#define each_vector_inner(item, array, type, length)                           \
  type ptr = (type)(array), (item) = ptr;                                      \
  ptr < ((type)((array))) + (length);                                          \
  ptr += 1, (item) = ptr

#define each_vector(item, vector)                                              \
  each_vector_inner(item, (vector)->internal.ptr, typeof(((vector)->payload)), \
                    (vector)->internal.len)
#endif
