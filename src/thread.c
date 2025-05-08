#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "thread.h"
#include "utils.h"

static void *thread_start(void *arg) {
  ThreadPoolState *info = arg;

  while (1) {
    if (!atomic_load(info->is_active)) {
      break;
    }

    void *task = pop_task(&info->queue);
    if (task != NULL) {
      // work on task
      info->fn(task);
      continue;
    }

    // wait until queue has something to do
    pthread_mutex_lock(&info->queue.mutex);
    pthread_cond_wait(&info->queue.cond, &info->queue.mutex);
    pthread_mutex_unlock(&info->queue.mutex);
  }

  return NULL;
}

ThreadPool init_threadpool(ThreadFunction fn, size_t size) {

  pthread_t *thread = calloc(size, sizeof(pthread_t));

  ThreadPoolState *state = malloc(sizeof(ThreadPoolState));
  state->is_active = malloc(sizeof(atomic_bool));

  atomic_store(state->is_active, true);
  state->queue = init_queue();
  state->fn = fn;

  ThreadPool pool = {
      .thread = thread,
      .state = state,
      .size = size,
  };

  for (size_t i = 0; i < pool.size; i += 1) {
    // INIT Threadpool
    pthread_create(&pool.thread[i], NULL, &thread_start, state);
  }
  return pool;
}

void add_threaded_task(ThreadPool *pool, void *task) {
  add_task(&pool->state->queue, task);
}

void free_threadpool(ThreadPool *pool) {
  atomic_store(pool->state->is_active, false);

  // wake all threads
  pthread_cond_broadcast(&pool->state->queue.cond);

  for (size_t i = 0; i < pool->size; i += 1) {
    pthread_join(pool->thread[i], NULL);
  }

  free_queue(&pool->state->queue);

  free(pool->state->is_active);
  free(pool->state);
  free(pool->thread);
}

ThreadQueue init_queue() {
  ThreadQueue queue = {
      .buffer = NULL,
      .head = 0,
      .tail = 0,
      .size = THREAD_TASK_QUEUE_SIZE,
  };

  // add some initial size
  queue.buffer = calloc(THREAD_TASK_QUEUE_SIZE, sizeof(ThreadTaskPayload));
  ASSERT(queue.buffer != NULL);

  pthread_mutex_init(&queue.mutex, NULL);
  pthread_cond_init(&queue.cond, NULL);

  return queue;
}

void free_queue(ThreadQueue *queue) {
  pthread_mutex_lock(&queue->mutex);

  while (queue->tail != queue->head) {
    free(queue->buffer[queue->tail]);
    queue->tail = (queue->tail + 1) % queue->size;
  }

  free(queue->buffer);

  queue->size = 0;
  queue->head = 0;
  queue->tail = 0;
  queue->buffer = NULL;

  pthread_mutex_unlock(&queue->mutex);
  pthread_mutex_destroy(&queue->mutex);
}

#include <stdio.h>

void move_head(ThreadQueue *queue) {

  // move up the head
  queue->head = (queue->head + 1) % queue->size;

  // we have not used up the whole buffer
  if (queue->head != queue->tail) {
    return;
  }

  size_t old_size = queue->size;
  queue->size *= 2;

  size_t sp = sizeof(ThreadTaskPayload);

  queue->buffer = realloc(queue->buffer, queue->size * sp);
  ASSERT(queue->buffer != NULL);

  memset(queue->buffer + old_size, 0, old_size);

  // move everything before tail to after the tail

  size_t elem_at_end = old_size - queue->tail;

  memcpy(queue->buffer + old_size, queue->buffer, elem_at_end * sp);
  memset(queue->buffer, 0, elem_at_end * sp);

  queue->head = (old_size + queue->tail) % queue->size;
}

void add_task(ThreadQueue *queue, void *task) {
  pthread_mutex_lock(&queue->mutex);
  ASSERT(queue->buffer != NULL);

  queue->buffer[queue->head] = task;
  move_head(queue);

  printf("head %zu - tail %zu - size %zu\n", queue->head, queue->tail,
         queue->size);

  pthread_mutex_unlock(&queue->mutex);

  // start one of the waiting threads
  pthread_cond_signal(&queue->cond);
}

void *pop_task(ThreadQueue *queue) {
  void *task = NULL;

  pthread_mutex_lock(&queue->mutex);
  if (queue->tail == queue->head) {
    goto POP_TASK_UNLOCK;
  }

  task = queue->buffer[queue->tail];
  queue->buffer[queue->tail] = NULL;

  queue->tail = (queue->tail + 1) % queue->size;

POP_TASK_UNLOCK:
  pthread_mutex_unlock(&queue->mutex);
  return task;
}
