#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <threads.h>

#include "utils.h"

#define Queue(X) Queue_##X
#define _UN __attribute__((unused))

#define INIT_QUEUE(X)                                                          \
  struct Queue(X) {                                                            \
    mtx_t mutex;                                                               \
    cnd_t cond;                                                                \
    X *buffer;                                                                 \
    size_t head;                                                               \
    size_t tail;                                                               \
    size_t size;                                                               \
  };                                                                           \
                                                                               \
  typedef struct Queue(X) Queue(X);                                            \
                                                                               \
  _UN static Queue(X) init_queue_##X(size_t size) {                            \
    Queue(X) queue = {                                                         \
        .buffer = NULL,                                                        \
        .head = 0,                                                             \
        .tail = 0,                                                             \
        .size = size,                                                          \
    };                                                                         \
                                                                               \
    /* add some initial size */                                                \
    queue.buffer = (X *)calloc(size, sizeof(X));                               \
    ASSERT(queue.buffer != NULL);                                              \
                                                                               \
    mtx_init(&queue.mutex, mtx_plain);                                         \
    cnd_init(&queue.cond);                                                     \
                                                                               \
    return queue;                                                              \
  }                                                                            \
                                                                               \
  _UN static void free_queue_##X(Queue(X) * queue) {                           \
    mtx_lock(&queue->mutex);                                                   \
                                                                               \
    while (queue->tail != queue->head) {                                       \
      free(queue->buffer[queue->tail]);                                        \
      queue->tail = (queue->tail + 1) % queue->size;                           \
    }                                                                          \
                                                                               \
    free(queue->buffer);                                                       \
                                                                               \
    queue->size = 0;                                                           \
    queue->head = 0;                                                           \
    queue->tail = 0;                                                           \
    queue->buffer = NULL;                                                      \
                                                                               \
    mtx_unlock(&queue->mutex);                                                 \
    mtx_destroy(&queue->mutex);                                                \
  }                                                                            \
                                                                               \
  _UN static bool is_full_##X(Queue(X) * queue) {                              \
    return queue->head == queue->tail;                                         \
  }                                                                            \
                                                                               \
  _UN static void move_head_##X(Queue(X) * queue) {                            \
                                                                               \
    /* move up the head */                                                     \
    queue->head = (queue->head + 1) % queue->size;                             \
                                                                               \
    /* we have not used up the whole buffer */                                 \
    if (!is_full_##X(queue)) {                                                 \
      return;                                                                  \
    }                                                                          \
                                                                               \
    size_t old_size = queue->size;                                             \
    queue->size *= 2;                                                          \
                                                                               \
    size_t sp = sizeof(Queue(X));                                              \
                                                                               \
    queue->buffer = (X *)realloc((void *)queue->buffer, queue->size * sp);     \
    ASSERT(queue->buffer != NULL);                                             \
                                                                               \
    memset(queue->buffer + old_size, 0, old_size);                             \
                                                                               \
    /* move everything before tail to after the tail*/                         \
                                                                               \
    size_t elem_at_end = old_size - queue->tail;                               \
                                                                               \
    memcpy(queue->buffer + old_size, queue->buffer, elem_at_end * sp);         \
    memset(queue->buffer, 0, elem_at_end * sp);                                \
                                                                               \
    queue->head = (old_size + queue->tail) % queue->size;                      \
  }                                                                            \
                                                                               \
  _UN static void add_task_##X(Queue(X) * queue, X task) {                     \
    mtx_lock(&queue->mutex);                                                   \
    ASSERT(queue->buffer != NULL);                                             \
                                                                               \
    queue->buffer[queue->head] = task;                                         \
    move_head_##X(queue);                                                      \
                                                                               \
    debug("head %zu - tail %zu - size %zu\n", queue->head, queue->tail,        \
          queue->size);                                                        \
                                                                               \
    mtx_unlock(&queue->mutex);                                                 \
                                                                               \
    /* start one of the waiting threads */                                     \
    cnd_signal(&queue->cond);                                                  \
  }                                                                            \
                                                                               \
  _UN static void *pop_task_##X(Queue(X) * queue) {                            \
    void *task = NULL;                                                         \
                                                                               \
    mtx_lock(&queue->mutex);                                                   \
    if (queue->tail == queue->head) {                                          \
      goto POP_TASK_UNLOCK_##X;                                                \
    }                                                                          \
                                                                               \
    task = queue->buffer[queue->tail];                                         \
    queue->buffer[queue->tail] = NULL;                                         \
                                                                               \
    queue->tail = (queue->tail + 1) % queue->size;                             \
                                                                               \
    POP_TASK_UNLOCK_##X : mtx_unlock(&queue->mutex);                           \
    return task;                                                               \
  }
#endif // !QUEUE_H
