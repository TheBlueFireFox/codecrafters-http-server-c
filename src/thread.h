#ifndef THREAD
#define THREAD

#include <stdatomic.h>
#include <stddef.h>
#include <threads.h>
#include <unistd.h>

#include "queue.h"

#define THREADPOOL_SIZE sysconf(_SC_NPROCESSORS_ONLN)
#define THREAD_TASK_QUEUE_SIZE THREADPOOL_SIZE

typedef void *ThreadTask;

typedef void (*ThreadFunction)(void *);

struct ThreadPoolState {
  Queue(ThreadTask) queue;
  mtx_t mutex;
  cnd_t cond;
  atomic_bool *is_active;
  ThreadFunction fn;
};

typedef struct ThreadPoolState ThreadPoolState;

struct ThreadPool {
  ThreadPoolState *state;
  thrd_t *thread;
  size_t size;
};
typedef struct ThreadPool ThreadPool;

ThreadPool init_threadpool(ThreadFunction fn, size_t size);
void add_threaded_task(ThreadPool *pool, void *task);

void free_threadpool(ThreadPool *pool);

#endif // !THREAD
