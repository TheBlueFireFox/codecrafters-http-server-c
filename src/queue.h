#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct QueueInternal {
  uint8_t *buffer;
  size_t size;
  size_t capacity;
  size_t head;
  size_t tail;
};

typedef struct QueueInternal QueueInternal;

#define queue_payload_size(obj) sizeof(*(obj)->payload)

#define Queue(type)                                                            \
  union {                                                                      \
    QueueInternal internal;                                                    \
    /* NOLINTNEXTLINE bugprone-macro-parentheses */                            \
    type *payload;                                                             \
  }

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions prepares the queue and preallocates the buffer to the capacity
void queue_init_impl(QueueInternal *queue, size_t capacity, size_t obj_size);

#define queue_init(queue, capacity)                                            \
  queue_init_impl((&(queue)->internal), capacity, queue_payload_size(queue));

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions frees the queue and resets everything to zero
void queue_free_impl(QueueInternal *queue);

#define queue_free(queue) queue_free_impl(&(queue)->internal);

bool queue_is_full_impl(QueueInternal *queue);

#define queue_is_full(queue) queue_is_full_impl(&(queue)->internal)

bool queue_is_empty_impl(QueueInternal *queue);

#define queue_is_empty(queue) queue_is_empty_impl(&(queue)->internal)

void queue_enqueue_impl(QueueInternal *queue, uint8_t const *val,
                        size_t obj_size);

#define queue_enqueue(queue, item)                                             \
  queue_enqueue_impl(&(queue)->internal,                                       \
                     (uint8_t const *const)(1 ? &(item) : ((queue)->payload)), \
                     queue_payload_size(queue))

bool queue_dequeue_impl(QueueInternal *queue, uint8_t *val, size_t obj_size);

#define queue_dequeue(queue, item)                                             \
  queue_dequeue_impl(&(queue)->internal,                                       \
                     (uint8_t *)(1 ? (item) : ((queue)->payload)),             \
                     queue_payload_size(queue))

#endif
