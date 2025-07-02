#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "thread.h"

static int thread_start(void *arg) {
  ThreadPoolState *info = arg;

  while (1) {
    if (!atomic_load(info->is_active)) {
      break;
    }

    void *task = pop_task_ThreadTask(&info->queue);
    if (task != NULL) {
      // work on task
      info->fn(task);
      continue;
    }

    // wait until queue has something to do
    mtx_lock(&info->queue.mutex);
    cnd_wait(&info->queue.cond, &info->queue.mutex);
    mtx_unlock(&info->queue.mutex);
  }

  return 0;
}

ThreadPool init_threadpool(ThreadFunction fn, size_t size) {

  thrd_t *thread = calloc(size, sizeof(thrd_t));

  ThreadPoolState *state = malloc(sizeof(ThreadPoolState));
  state->is_active = malloc(sizeof(atomic_bool));

  atomic_store(state->is_active, true);
  state->queue = init_queue_ThreadTask(THREAD_TASK_QUEUE_SIZE);
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
  add_task_ThreadTask(&pool->state->queue, task);
}

void free_threadpool(ThreadPool *pool) {
  atomic_store(pool->state->is_active, false);

  // wake all threads
  cnd_broadcast(&pool->state->queue.cond);

  for (size_t i = 0; i < pool->size; i += 1) {
    thrd_join(pool->thread[i], NULL);
  }

  free_queue_ThreadTask(&pool->state->queue);

  free(pool->state->is_active);
  free(pool->state);
  free(pool->thread);
}
