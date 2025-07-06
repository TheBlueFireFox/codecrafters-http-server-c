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
                               sizeof(*(vector)->payload));                    \
  } while (0)

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions frees the vector and resets everything to zero
void _free_vector(VectorInternal *vec);

#define free_vector(vector) _free_vector(&(vector)->internal);

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions adds an element to the vector
void _push_vector(VectorInternal *vec, uint8_t const *const elem,
                  size_t obj_size);

#define push_vector(vector, item)                                              \
  _push_vector((&(vector)->internal),                                          \
               (uint8_t const *const)(1 ? &(item) : ((vector)->payload)),      \
               sizeof(*((&(vector)->payload))))

typedef int (*compFn)(const void *p1, const void *p2);

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions will sort the whole vector inplace using the qsort function
void _sort_vector(VectorInternal *vec, size_t obj_size, compFn comp_fn);

#define sort_vector(vector, comp_fn)                                           \
  _sort_vector((&(vector)->internal), sizeof(*((&(vector)->payload))), comp_fn)

void *_get_vector(VectorInternal *vec, size_t idx, size_t obj_size);

#define get_vector(vector, idx)                                                \
  (typeof(((vector)->payload)))(_get_vector(&(vector)->internal, idx,          \
                                            sizeof(*((&(vector)->payload)))))

#endif
