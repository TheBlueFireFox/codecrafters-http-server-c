#include "thread.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

static void *thread_start(void *arg) {
  ThreadPoolState *info = arg;

  while (1) {
    bool is_active = false;
    pthread_rwlock_rdlock(&info->mutex);
    is_active = info->is_active;
    pthread_rwlock_unlock(&info->mutex);

    if (!is_active) {
      break;
    }

    void *task = pop_task(&info->queue);
    if (task != NULL) {
      // work on task
      info->fn(task);
    }

    // wait until queue has something to do
    pthread_mutex_lock(&info->queue.mutex);
    pthread_cond_wait(&info->queue.cond, &info->queue.mutex);
    pthread_mutex_unlock(&info->queue.mutex);
  }

  return NULL;
}

ThreadPool init_threadpool(ThreadFunction fn) {

  ThreadPoolState *state = malloc(sizeof(ThreadPoolState));
  state->is_active = true;
  state->mutex = (pthread_rwlock_t){};
  state->queue = init_queue();
  state->fn = fn;

  pthread_rwlock_init(&state->mutex, NULL);

  ThreadPool pool = {
      .thread = {0},
      .state = state,
  };

  for (size_t i = 0; i < THREADPOOL_SIZE; i += 1) {
    // INIT Threadpool
    pthread_create(&pool.thread[i], NULL, &thread_start, state);
  }
  return pool;
}

void add_threaded_task(ThreadPool *pool, void *task) {
  add_task(&pool->state->queue, task);
}

void free_threadpool(ThreadPool *pool) {
  pthread_rwlock_wrlock(&pool->state->mutex);
  pool->state->is_active = false;
  pthread_rwlock_unlock(&pool->state->mutex);

  // wake all threads
  pthread_cond_broadcast(&pool->state->queue.cond);

  for (size_t i = 0; i < THREADPOOL_SIZE; i += 1) {
    pthread_join(pool->thread[i], NULL);
  }

  free_queue(&pool->state->queue);
  pthread_rwlock_destroy(&pool->state->mutex);
  free(pool->state);
}

ThreadQueue init_queue() {
  ThreadQueue queue = {
      .cond = {},
      .mutex = {},
      .head = NULL,
      .last = NULL,
  };

  pthread_mutex_init(&queue.mutex, NULL);
  pthread_cond_init(&queue.cond, NULL);

  return queue;
}

void free_queue(ThreadQueue *queue) {
  pthread_mutex_lock(&queue->mutex);
  while (queue->head != NULL) {

    if (queue->head->payload != NULL) {
      free(queue->head->payload);
      queue->head->payload = NULL;
    }

    ThreadTask *curr = queue->head;
    queue->head = curr->next;
    free(curr);
  }

  queue->head = NULL;
  queue->last = NULL;

  pthread_mutex_unlock(&queue->mutex);
  pthread_mutex_destroy(&queue->mutex);
}

void add_task(ThreadQueue *queue, void *task) {
  pthread_mutex_lock(&queue->mutex);
  ThreadTask *task_node = malloc(sizeof(ThreadTask));
  task_node->payload = task;
  task_node->next = NULL;
  queue->last = task_node;

  if (queue->head == NULL) {
    queue->head = task_node;
  }
  pthread_mutex_unlock(&queue->mutex);

  // start one of the waiting threads
  pthread_cond_signal(&queue->cond);
}

void *pop_task(ThreadQueue *queue) {
  void* task = NULL;
  ThreadTask *task_node = NULL;

  pthread_mutex_lock(&queue->mutex);
  if (queue->head == NULL) {
    goto POP_CLEAN_UP;
  }

  task_node = queue->head;
  task = task_node->payload;
  queue->head = task_node->next;

  if (queue->head == NULL) {
    queue->last = NULL;
  }
POP_CLEAN_UP:
  pthread_mutex_unlock(&queue->mutex);
  if (task_node != NULL) {
    free(task_node);
  }
  return task;
}
