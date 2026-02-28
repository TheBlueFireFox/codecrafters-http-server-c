#include <stdlib.h>
#include <string.h>

#include "queue.h"
#include "utils.h"

void queue_init_impl(QueueInternal *queue, size_t capacity, size_t obj_size) {
  queue->buffer = NULL;
  queue->head = 0;
  queue->tail = 0;
  queue->size = 0;
  queue->capacity = capacity;

  // add some initial capacity
  queue->buffer = malloc(capacity * obj_size);
  ASSERT(queue->buffer != NULL);
}

void queue_free_impl(QueueInternal *queue) {
  free(queue->buffer);

  queue->buffer = NULL;
  queue->size = 0;
  queue->capacity = 0;
  queue->head = 0;
  queue->tail = 0;
}

bool queue_is_full_impl(QueueInternal *queue) {
  return queue->capacity == queue->size;
}

bool queue_is_empty_impl(QueueInternal *queue) { return queue->size == 0; }

static void resize_queue(QueueInternal *queue, size_t obj_size) {
  // no need to resize queue
  if (!queue_is_full_impl(queue)) {
    return;
  }

  debug("RESIZE head %zu - tail %zu - size %zu\n", queue->head, queue->tail,
        queue->size);

  size_t old_capacity = queue->capacity;

  queue->capacity *= 2;

  void *buf = realloc(queue->buffer, queue->capacity * obj_size);
  ASSERT(buf != NULL);

  queue->buffer = buf;

  if (queue->tail > queue->head) {
    return;
  }
  // move everything before tail to after the tail

  size_t elem_at_start = queue->tail;

  memcpy(queue->buffer + (old_capacity * obj_size), queue->buffer,
         elem_at_start * obj_size);
  memset(queue->buffer, 0, elem_at_start * obj_size);

  queue->tail = (old_capacity + queue->head) % queue->capacity;
}

static void move_head_queue(QueueInternal *queue) {
  queue->head = (queue->head + 1) % queue->capacity;
}

static void move_tail_queue(QueueInternal *queue) {
  queue->tail = (queue->tail + 1) % queue->capacity;
}

void queue_enqueue_impl(QueueInternal *queue, uint8_t const *const val,
                        size_t obj_size) {
  resize_queue(queue, obj_size);
  queue->size += 1;

  memcpy(queue->buffer + (queue->tail * obj_size), val, obj_size);
  move_tail_queue(queue);
  debug("head %zu - tail %zu - size %zu\n", queue->head, queue->tail,
        queue->size);
}

bool queue_dequeue_impl(QueueInternal *queue, uint8_t *val, size_t obj_size) {
  if (queue_is_empty_impl(queue)) {
    return false;
  }
  queue->size -= 1;

  ASSERT(val != NULL);
  memcpy(val, queue->buffer + (queue->head * obj_size), obj_size);
  move_head_queue(queue);
  debug("head %zu - tail %zu - size %zu\n", queue->head, queue->tail,
        queue->size);
  return true;
}
