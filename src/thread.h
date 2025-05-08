#ifndef THREAD
#define THREAD

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define THREADPOOL_SIZE 8
#define THREAD_TASK_QUEUE_SIZE 64

typedef void *ThreadTaskPayload;

struct ThreadTaskQueue {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  ThreadTaskPayload *buffer;
  size_t head;
  size_t tail;
  size_t size;
};

typedef struct ThreadTaskQueue ThreadQueue;

ThreadQueue init_queue();

void free_queue(ThreadQueue *queue);

void add_task(ThreadQueue *queue, void *task);

void *pop_task(ThreadQueue *queue);

typedef void (*ThreadFunction)(void *);

struct ThreadPoolState {
  bool is_active;
  pthread_rwlock_t mutex;
  ThreadQueue queue;
  ThreadFunction fn;
};

typedef struct ThreadPoolState ThreadPoolState;

struct ThreadPool {
  ThreadPoolState *state;
  pthread_t *thread;
  size_t size;
};
typedef struct ThreadPool ThreadPool;

ThreadPool init_threadpool(ThreadFunction fn, size_t size);
void add_threaded_task(ThreadPool *pool, void *task);

void free_threadpool(ThreadPool *pool);

#endif // !THREAD
