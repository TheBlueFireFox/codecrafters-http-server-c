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
    mtx_lock(&info->mutex);
    bool has = dequeue_queue(&info->queue, &task);
    mtx_unlock(&info->mutex);

    if (has) {
      ASSERT(task != NULL);
      // work on task
      info->fn(task);
      continue;
    }

    // wait until queue has something to do
    mtx_lock(&info->mutex);
    cnd_wait(&info->cond, &info->mutex);
    mtx_unlock(&info->mutex);
  }

  return 0;
}

ThreadPool init_threadpool(ThreadFunction fn, size_t size) {

  thrd_t *thread = calloc(size, sizeof(thrd_t));

  ThreadPoolState *state = malloc(sizeof(ThreadPoolState));
  state->is_active = false;

  mtx_init(&state->mutex, mtx_plain);
  cnd_init(&state->cond);

  atomic_store(&state->is_active, true);
  init_queue(&state->queue, THREAD_TASK_QUEUE_SIZE);
  state->fn = fn;

  ThreadPool pool = {
      .thread = thread,
      .state = state,
      .size = size,
  };

  for (size_t i = 0; i < pool.size; i += 1) {
    // INIT Threadpool
    thrd_create(&pool.thread[i], &thread_start, state);
  }
  return pool;
}

void add_threaded_task(ThreadPool *pool, void *task) {
  mtx_lock(&pool->state->mutex);
  enqueue_queue(&pool->state->queue, task);
  mtx_unlock(&pool->state->mutex);

  /* start one of the waiting threads */
  cnd_signal(&pool->state->cond);
}

void free_threadpool(ThreadPool *pool) {
  atomic_store(&pool->state->is_active, false);

  // wake all threads
  cnd_broadcast(&pool->state->cond);

  for (size_t i = 0; i < pool->size; i += 1) {
    thrd_join(pool->thread[i], NULL);
  }

  // TODO: loop over all the queue elements and free them
  mtx_lock(&pool->state->mutex);
  free_queue(&pool->state->queue);
  mtx_unlock(&pool->state->mutex);
  mtx_destroy(&pool->state->mutex);

  free(pool->state);
  free(pool->thread);
}
