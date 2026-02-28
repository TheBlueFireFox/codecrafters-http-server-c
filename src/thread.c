#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "thread.h"
#include "utils.h"

static int thread_start(void *arg) {
  ThreadPoolState *info = arg;

  while (1) {
    if (!atomic_load(&info->is_active)) {
      break;
    }

    void *task = NULL;
    (void)mtx_lock(&info->mutex);
    bool has = queue_dequeue(&info->queue, &task);
    (void)mtx_unlock(&info->mutex);

    if (has) {
      ASSERT(task != NULL);
      // work on task
      info->fn(task);
      continue;
    }

    // wait until queue has something to do
    (void)mtx_lock(&info->mutex);
    (void)cnd_wait(&info->cond, &info->mutex);
    (void)mtx_unlock(&info->mutex);
  }

  return 0;
}

ThreadPool init_threadpool(ThreadFunction fn, size_t size) {

  thrd_t *thread = calloc(size, sizeof(thrd_t));

  ThreadPoolState *state = malloc(sizeof(ThreadPoolState));
  state->is_active = false;

  (void)mtx_init(&state->mutex, mtx_plain);
  (void)cnd_init(&state->cond);

  atomic_store(&state->is_active, true);
  queue_init(&state->queue, THREAD_TASK_QUEUE_SIZE);
  state->fn = fn;

  ThreadPool pool = {
      .thread = thread,
      .state = state,
      .size = size,
  };

  for (size_t i = 0; i < pool.size; i += 1) {
    // INIT Threadpool
    (void)thrd_create(&pool.thread[i], &thread_start, state);
  }
  return pool;
}

void add_threaded_task(ThreadPool *pool, void *task) {
  (void)mtx_lock(&pool->state->mutex);
  queue_enqueue(&pool->state->queue, task);
  (void)mtx_unlock(&pool->state->mutex);

  /* start one of the waiting threads */
  (void)cnd_signal(&pool->state->cond);
}

void free_threadpool(ThreadPool *pool) {
  atomic_store(&pool->state->is_active, false);

  // wake all threads
  (void)cnd_broadcast(&pool->state->cond);

  for (size_t i = 0; i < pool->size; i += 1) {
    (void)thrd_join(pool->thread[i], NULL);
  }

  (void)mtx_lock(&pool->state->mutex);
  while (!queue_is_empty(&pool->state->queue)) {
    ThreadTask tt;
    queue_dequeue(&pool->state->queue, &tt);
    free(tt);
  }
  queue_free(&pool->state->queue);
  (void)mtx_unlock(&pool->state->mutex);
  mtx_destroy(&pool->state->mutex);

  free(pool->state);
  free(pool->thread);
}
