#ifndef VECTOR_H
#define VECTOR_H

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
    type *payload;                                                             \
  }

#define _payload_size(vector) sizeof(*(vector)->payload)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions prepares the vector and does not allocate
void _init_vector(VectorInternal *vec);

#define init_vector(vector)                                                    \
  do {                                                                         \
    _init_vector((&(vector)->internal));                                       \
  } while (0)

// Internal function for vector this should not be used directly by anything
// except by the public macros
//
// This functions initialized the vector and preallocated capacity for
// the vector
void _init_vector_with_capacity(VectorInternal *vec, size_t capacity,
                                size_t obj_size);

#define init_vector_with_capacity(vector, capacity)                            \
  do {                                                                         \
    _init_vector_with_capacity((&(vector)->internal), capacity,                \
                               _payload_size(vector));                         \
  } while (0)

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions frees the vector and resets everything to zero
void _free_vector(VectorInternal *vec);

#define free_vector(vector) _free_vector(&(vector)->internal)

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions return the vectors len
size_t _len_vector(VectorInternal *vec);

#define len_vector(vector) _len_vector(&(vector)->internal)

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions adds an element to the vector
void _push_vector(VectorInternal *vec, uint8_t const *const elem,
                  size_t obj_size);

#define push_vector(vector, item)                                              \
  _push_vector((&(vector)->internal),                                          \
               (uint8_t const *const)(1 ? &(item) : ((vector)->payload)),      \
               _payload_size(vector))

bool _get_vector(VectorInternal *vec, size_t idx, uint8_t *const val,
                 size_t obj_size);

#define get_vector(vector, idx, item)                                          \
  _get_vector(&(vector)->internal, idx,                                        \
              (uint8_t *const)(1 ? (item) : ((vector)->payload)),              \
              _payload_size(vector))

typedef int (*compFn)(const void *p1, const void *p2);

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions will sort the whole vector inplace using the qsort function
void _sort_vector(VectorInternal *vec, size_t obj_size, compFn comp_fn);

#define sort_vector(vector, comp_fn)                                           \
  _sort_vector((&(vector)->internal), _payload_size(vector), comp_fn)

void *_search_vector(VectorInternal *vec, size_t obj_size, const void *key,
                     compFn comp_fn);

#define search_vector(vector, key, comp_fn)                                    \
  _search_vector((&(vector)->internal), _payload_size(vector), key, comp_fn)

// C23 extention typeof
#define _each_vector(item, array, type, length)                                \
  type p = (type)(array), (item) = p;                                          \
  p < ((type)((array))) + length;                                              \
  p += 1, (item) = p

#define each_vector(item, vector)                                              \
  _each_vector(item, (vector)->internal.ptr, typeof(((vector)->payload)),      \
               (vector)->internal.len)
#endif
