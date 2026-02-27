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
void init_queue_impl(QueueInternal *queue, size_t capacity, size_t obj_size);

#define init_queue(queue, capacity)                                            \
  init_queue_impl((&(queue)->internal), capacity, queue_payload_size(queue));

// Internal function for queue this should not be used directly by anything
// except by the public macros
//
// This functions frees the queue and resets everything to zero
void free_queue_impl(QueueInternal *queue);

#define free_queue(queue) free_queue_impl(&(queue)->internal);

bool is_full_queue_impl(QueueInternal *queue);

#define is_full_queue(queue) is_full_queue_impl(&(queue)->internal)

bool is_empty_queue_impl(QueueInternal *queue);

#define is_empty_queue(queue) is_empty_queue_impl(&(queue)->internal)

void enqueue_queue_impl(QueueInternal *queue, uint8_t const *val,
                        size_t obj_size);

#define enqueue_queue(queue, item)                                             \
  enqueue_queue_impl(&(queue)->internal,                                       \
                     (uint8_t const *const)(1 ? &(item) : ((queue)->payload)), \
                     queue_payload_size(queue))

bool dequeue_queue_impl(QueueInternal *queue, uint8_t *val, size_t obj_size);

#define dequeue_queue(queue, item)                                             \
  dequeue_queue_impl(&(queue)->internal,                                       \
                     (uint8_t *)(1 ? (item) : ((queue)->payload)),             \
                     queue_payload_size(queue))

#endif
