#ifndef THREAD
#define THREAD

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

struct ThreadTask {
  void *payload;
  struct ThreadTask *next;
};

typedef struct ThreadTask ThreadTask;

struct ThreadTaskQueue {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  ThreadTask *head;
  ThreadTask *last;
};

typedef struct ThreadTaskQueue ThreadQueue;

ThreadQueue init_queue();

void free_queue(ThreadQueue *queue);

void add_task(ThreadQueue *queue, void *task);

void *pop_task(ThreadQueue *queue);

typedef void (*ThreadFunction)(void *);

#define THREADPOOL_SIZE 8

struct ThreadPoolState {
  bool is_active;
  pthread_rwlock_t mutex;
  ThreadQueue queue;
  ThreadFunction fn;
};

typedef struct ThreadPoolState ThreadPoolState;

struct ThreadPool {
  ThreadPoolState *state;
  pthread_t thread[THREADPOOL_SIZE];
};
typedef struct ThreadPool ThreadPool;

ThreadPool init_threadpool(ThreadFunction fn);
void add_threaded_task(ThreadPool *pool, void *task);

void free_threadpool(ThreadPool *pool);

#endif // !THREAD
