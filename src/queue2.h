#ifndef QUEUE2_H
#define QUEUE2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct QueueInternal {
  uint8_t *buffer;
  size_t elem_size;
  size_t size;
  size_t capacity;
  size_t head;
  size_t tail;
};

typedef struct QueueInternal QueueInternal;

#define Queue(type)                                                            \
  union {                                                                      \
    QueueInternal *internal;                                                   \
    type *payload;                                                             \
  }

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions prepares the queue and preallocates the buffer to the capacity
void _init_queue(QueueInternal *qi, size_t capacity, size_t obj_size);

#define init_queue(queue, capacity)                                            \
  do {                                                                         \
    (queue)->internal = (QueueInternal *)malloc(sizeof(QueueInternal));        \
    _init_queue(((queue)->internal), capacity, sizeof(*(queue)->payload));     \
  } while (0)

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions frees the queue and resets everything to zero
void _free_queue(QueueInternal *qi);

#define free_queue(queue)                                                      \
  do {                                                                         \
    _free_queue((queue)->internal);                                            \
    free((queue)->internal);                                                   \
    (queue)->internal = NULL;                                                  \
  } while (0)

bool _is_full_queue(QueueInternal *qi);

#define is_full_queue(queue) _is_full_queue((queue)->internal)

bool _is_empty_queue(QueueInternal *qi);

#define is_empty_queue(queue) _is_empty_queue((queue)->internal)

void _enqueue_queue(QueueInternal *qi, uint8_t const *const val);

#define enqueue_queue(queue, item)                                             \
  _enqueue_queue((queue)->internal,                                            \
                 (uint8_t const *const)(1 ? &(item) : ((queue)->payload)))

bool _dequeue_queue(QueueInternal *qi, uint8_t *const val);

#define dequeue_queue(queue, item)                                             \
  _dequeue_queue((queue)->internal,                                            \
                 (uint8_t *const)(1 ? (item) : ((queue)->payload)))

#endif // !QUEUE2_H
