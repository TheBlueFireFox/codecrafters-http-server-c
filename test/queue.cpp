#include <gtest/gtest.h>

extern "C" {
#include "queue.h"
}

TEST(Test, isEmpty) {
  Queue(uint8_t) queue;

  init_queue(&queue, 4);

  bool r = is_full_queue(&queue);
  EXPECT_EQ(r, false);

  bool l = is_empty_queue(&queue);
  EXPECT_EQ(l, true);

  free_queue(&queue);
}

TEST(Test, enqueue) {
  Queue(uint32_t) queue;

  init_queue(&queue, 4);

  uint32_t exp = 0xFF55FF55;
  enqueue_queue(&queue, exp);

  EXPECT_EQ(queue.internal->head, 0);
  EXPECT_EQ(queue.internal->tail, 1);

  uint32_t got = *(uint32_t *)queue.internal->buffer;

  EXPECT_EQ(got, exp);

  free_queue(&queue);
}

TEST(Test, dequeue) {
  Queue(uint32_t) queue;

  init_queue(&queue, 4);

  uint32_t got = 0;

  bool task = dequeue_queue(&queue, &got);
  EXPECT_EQ(task, false);

  uint32_t exp = 0xFF55FF55;
  enqueue_queue(&queue, exp);

  task = dequeue_queue(&queue, &got);

  EXPECT_EQ(task, true);

  EXPECT_EQ(queue.internal->head, 1);
  EXPECT_EQ(queue.internal->tail, 1);

  EXPECT_EQ(got, exp);

  free_queue(&queue);
}

TEST(Test, utilizeMax) {
  Queue(size_t) queue;

  init_queue(&queue, 4);

  for (size_t i = 0; i < 4; i += 1) {
    enqueue_queue(&queue, i);
  }

  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 4);
  EXPECT_EQ(queue.internal->head, 0);
  EXPECT_EQ(queue.internal->tail, 0);

  for (size_t i = 0; i < 4; i += 1) {
    size_t got = ~0;
    size_t *got_ptr = &got;
    bool task = dequeue_queue(&queue, got_ptr);
    EXPECT_EQ(task, true);
    EXPECT_EQ(got, i);
  }

  EXPECT_EQ(queue.internal->tail, 0);
  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 0);

  free_queue(&queue);
}

TEST(Test, wrapAround) {
  Queue(size_t) queue;

  init_queue(&queue, 4);

  for (size_t i = 0; i < 4; i += 1) {
    enqueue_queue(&queue, i);
  }

  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 4);
  EXPECT_EQ(queue.internal->head, 0);
  EXPECT_EQ(queue.internal->tail, 0);

  for (size_t i = 0; i < 4; i += 1) {
    size_t got = ~0;
    size_t *got_ptr = &got;
    dequeue_queue(&queue, got_ptr);
    EXPECT_EQ(got, i);
  }

  EXPECT_EQ(queue.internal->head, 0);
  EXPECT_EQ(queue.internal->tail, 0);
  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 0);

  for (size_t i = 0; i < 2; i += 1) {
    enqueue_queue(&queue, i);
  }

  for (size_t i = 0; i < 2; i += 1) {
    size_t got = ~0;
    size_t *got_ptr = &got;
    dequeue_queue(&queue, got_ptr);
    EXPECT_EQ(got, i);
  }

  EXPECT_EQ(queue.internal->head, 2);
  EXPECT_EQ(queue.internal->tail, 2);
  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 0);

  free_queue(&queue);
}

TEST(Test, resizeBuffer) {
  Queue(size_t) queue;

  init_queue(&queue, 4);

  for (size_t i = 0; i < 4; i += 1) {
    enqueue_queue(&queue, i);
  }

  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 4);
  EXPECT_EQ(queue.internal->head, 0);
  EXPECT_EQ(queue.internal->tail, 0);

  size_t got = *(size_t *)queue.internal->buffer;

  EXPECT_EQ(got, 0);

  size_t ff = 42;
  enqueue_queue(&queue, ff);

  EXPECT_EQ(queue.internal->capacity, 8);
  EXPECT_EQ(queue.internal->size, 5);
  EXPECT_EQ(queue.internal->head, 0);
  EXPECT_EQ(queue.internal->tail, 5);

  free_queue(&queue);
}

TEST(Test, resizeBuffer2) {
  Queue(size_t) queue;

  init_queue(&queue, 4);

  // fill up to the end
  for (size_t i = 0; i < 4; i += 1) {
    enqueue_queue(&queue, i);
  }

  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 4);
  EXPECT_EQ(queue.internal->head, 0);
  EXPECT_EQ(queue.internal->tail, 0);

  size_t got = *(size_t *)queue.internal->buffer;

  EXPECT_EQ(got, 0);

  // empty down to the end - 1 to move head to the end
  for (size_t i = 0; i < 3; i += 1) {
    size_t got = ~0;
    size_t *got_ptr = &got;
    dequeue_queue(&queue, got_ptr);
    EXPECT_EQ(got, i);
  }

  // fill back up and have head and tail point to the
  // same
  for (size_t i = 0; i < 3; i += 1) {
    enqueue_queue(&queue, i);
  }

  EXPECT_EQ(queue.internal->capacity, 4);
  EXPECT_EQ(queue.internal->size, 4);
  EXPECT_EQ(queue.internal->head, 3);
  EXPECT_EQ(queue.internal->tail, 3);

  // force a buffer upgrade
  size_t ff = 42;
  enqueue_queue(&queue, ff);

  EXPECT_EQ(queue.internal->capacity, 8);
  EXPECT_EQ(queue.internal->size, 5);
  EXPECT_EQ(queue.internal->head, 3);
  EXPECT_EQ(queue.internal->tail, 0);

  free_queue(&queue);
}
