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

#define vector_payload_size(obj) sizeof(*(obj)->payload)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions prepares the vector and does not allocate
void vector_init_impl(VectorInternal *vec);

#define vector_init(vector) vector_init_impl((&(vector)->internal));

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions initialized the vector and preallocated capacity for
// the vector
void vector_init_with_capacity_impl(VectorInternal *vec, size_t capacity,
                                    size_t obj_size);

#define vector_init_with_capacity(vector, capacity)                            \
  vector_init_with_capacity_impl((&(vector)->internal), capacity,              \
                                 vector_payload_size(vector));

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions frees the vector and resets everything to zero
void vector_free_impl(VectorInternal *vec);

#define vector_free(vector) vector_free_impl(&(vector)->internal)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions return the vectors len
size_t vector_len_impl(VectorInternal *vec);

#define vector_len(vector) vector_len_impl(&(vector)->internal)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions resets the vector size
void vector_clear_impl(VectorInternal *vec);

#define vector_clear(vector) vector_clear_impl(&(vector)->internal)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions adds an element to the vector
void vector_push_impl(VectorInternal *vec, void const *elem, size_t obj_size);

#define vector_push(vector, item)                                              \
  vector_push_impl((&(vector)->internal), (1 ? &(item) : ((vector)->payload)), \
                   vector_payload_size(vector))

bool vector_get_impl(VectorInternal *vec, size_t idx, uint8_t *val,
                     size_t obj_size);

#define vector_get(vector, idx, item)                                          \
  vector_get_impl(&(vector)->internal, idx,                                    \
                  (uint8_t *const)(1 ? (item) : ((vector)->payload)),          \
                  vector_payload_size(vector))

typedef int (*compFn)(const void *param1, const void *param2);

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions will sort the whole vector inplace using the qsort function
void vector_sort_impl(VectorInternal *vec, size_t obj_size, compFn comp_fn);

#define vector_sort(vector, comp_fn)                                           \
  vector_sort_impl((&(vector)->internal), vector_payload_size(vector), comp_fn)

void *vector_search_impl(VectorInternal *vec, size_t obj_size, const void *key,
                         compFn comp_fn);

#define vector_search(vector, key, comp_fn)                                    \
  vector_search_impl((&(vector)->internal), vector_payload_size(vector),       \
                     (const void *)(key), (comp_fn))

// C23 extention typeof
#define vector_each_inner(item, array, type, length)                           \
  type ptr = (type)(array), (item) = ptr;                                      \
  ptr < ((type)((array))) + (length);                                          \
  ptr += 1, (item) = ptr

#define vector_each(item, vector)                                              \
  vector_each_inner(item, (vector)->internal.ptr, typeof(((vector)->payload)), \
                    (vector)->internal.len)
#endif
