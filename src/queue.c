#include <string.h>

#include "queue.h"
#include "utils.h"

void _init_queue(QueueInternal *qi, size_t capacity, size_t obj_size) {
  qi->buffer = NULL;
  qi->head = 0;
  qi->tail = 0;
  qi->size = 0;
  qi->capacity = capacity;

  // add some initial capacity
  qi->buffer = malloc(capacity * obj_size);
  ASSERT(qi->buffer != NULL);
}

void _free_queue(QueueInternal *qi) {
  free(qi->buffer);

  qi->buffer = NULL;
  qi->size = 0;
  qi->capacity = 0;
  qi->head = 0;
  qi->tail = 0;
}

bool _is_full_queue(QueueInternal *qi) { return qi->capacity == qi->size; }

bool _is_empty_queue(QueueInternal *qi) { return qi->size == 0; }

static void _resize_queue(QueueInternal *qi, size_t obj_size) {
  // no need to resize queue
  if (!_is_full_queue(qi)) {
    return;
  }

  debug("RESIZE head %zu - tail %zu - size %zu\n", qi->head, qi->tail,
        qi->size);

  size_t old_capacity = qi->capacity;

  qi->capacity *= 2;

  qi->buffer = realloc(qi->buffer, qi->capacity * obj_size);
  ASSERT(qi->buffer != NULL);

  if (qi->tail > qi->head) {
    return;
  }
  // move everything before tail to after the tail

  size_t elem_at_start = qi->tail;

  memcpy(qi->buffer + (old_capacity * obj_size), qi->buffer,
         elem_at_start * obj_size);
  memset(qi->buffer, 0, elem_at_start * obj_size);

  qi->tail = (old_capacity + qi->head) % qi->capacity;
}

static void _move_head_queue(QueueInternal *qi) {
  qi->head = (qi->head + 1) % qi->capacity;
}

static void _move_tail_queue(QueueInternal *qi) {
  qi->tail = (qi->tail + 1) % qi->capacity;
}

void _enqueue_queue(QueueInternal *qi, uint8_t const *const val,
                    size_t obj_size) {
  _resize_queue(qi, obj_size);
  qi->size += 1;

  memcpy(qi->buffer + qi->tail * obj_size, val, obj_size);
  _move_tail_queue(qi);
  debug("head %zu - tail %zu - size %zu\n", qi->head, qi->tail, qi->size);
}

bool _dequeue_queue(QueueInternal *qi, uint8_t *val, size_t obj_size) {
  if (_is_empty_queue(qi)) {
    return false;
  }
  qi->size -= 1;

  ASSERT(val != NULL);
  memcpy(val, qi->buffer + qi->head * obj_size, obj_size);
  _move_head_queue(qi);
  debug("head %zu - tail %zu - size %zu\n", qi->head, qi->tail, qi->size);
  return true;
}
