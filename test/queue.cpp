#include <gtest/gtest.h>

extern "C" {
#include "queue.h"
}

TEST(TestQueue, isEmpty) {
  Queue(uint8_t) queue;

  queue_init(&queue, 4);

  bool r = queue_is_full(&queue);
  ASSERT_EQ(r, false);

  bool l = queue_is_empty(&queue);
  ASSERT_EQ(l, true);

  queue_free(&queue);
}

TEST(TestQueue, enqueue) {
  Queue(uint32_t) queue;

  queue_init(&queue, 4);

  uint32_t exp = 0xFF55FF55;
  queue_enqueue(&queue, exp);

  ASSERT_EQ(queue.internal.head, 0);
  ASSERT_EQ(queue.internal.tail, 1);

  uint32_t got = *(uint32_t *)queue.internal.buffer;

  ASSERT_EQ(got, exp);

  queue_free(&queue);
}

TEST(TestQueue, dequeue) {
  Queue(uint32_t) queue;

  queue_init(&queue, 4);

  uint32_t got = 0;

  bool task = queue_dequeue(&queue, &got);
  ASSERT_EQ(task, false);

  uint32_t exp = 0xFF55FF55;
  queue_enqueue(&queue, exp);

  task = queue_dequeue(&queue, &got);

  ASSERT_EQ(task, true);

  ASSERT_EQ(queue.internal.head, 1);
  ASSERT_EQ(queue.internal.tail, 1);

  ASSERT_EQ(got, exp);

  queue_free(&queue);
}

TEST(TestQueue, utilizeMax) {
  Queue(size_t) queue;

  queue_init(&queue, 4);

  for (size_t i = 0; i < 4; i += 1) {
    queue_enqueue(&queue, i);
  }

  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 4);
  ASSERT_EQ(queue.internal.head, 0);
  ASSERT_EQ(queue.internal.tail, 0);

  for (size_t i = 0; i < 4; i += 1) {
    size_t got = ~0;
    bool task = queue_dequeue(&queue, &got);
    ASSERT_EQ(task, true);
    ASSERT_EQ(got, i);
  }

  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 0);
  ASSERT_EQ(queue.internal.tail, 0);

  queue_free(&queue);
}

TEST(TestQueue, wrapAround) {
  Queue(size_t) queue;

  queue_init(&queue, 4);

  for (size_t i = 0; i < 4; i += 1) {
    queue_enqueue(&queue, i);
  }

  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 4);
  ASSERT_EQ(queue.internal.head, 0);
  ASSERT_EQ(queue.internal.tail, 0);

  for (size_t i = 0; i < 4; i += 1) {
    size_t got = ~0;
    size_t *got_ptr = &got;
    queue_dequeue(&queue, got_ptr);
    ASSERT_EQ(got, i);
  }

  ASSERT_EQ(queue.internal.head, 0);
  ASSERT_EQ(queue.internal.tail, 0);
  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 0);

  for (size_t i = 0; i < 2; i += 1) {
    queue_enqueue(&queue, i);
  }

  for (size_t i = 0; i < 2; i += 1) {
    size_t got = ~0;
    queue_dequeue(&queue, &got);
    ASSERT_EQ(got, i);
  }

  ASSERT_EQ(queue.internal.head, 2);
  ASSERT_EQ(queue.internal.tail, 2);
  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 0);

  queue_free(&queue);
}

TEST(TestQueue, resizeBuffer) {
  Queue(size_t) queue;

  queue_init(&queue, 4);

  for (size_t i = 0; i < 4; i += 1) {
    queue_enqueue(&queue, i);
  }

  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 4);
  ASSERT_EQ(queue.internal.head, 0);
  ASSERT_EQ(queue.internal.tail, 0);

  size_t got = *(size_t *)queue.internal.buffer;

  ASSERT_EQ(got, 0);

  size_t ff = 42;
  queue_enqueue(&queue, ff);

  ASSERT_EQ(queue.internal.capacity, 8);
  ASSERT_EQ(queue.internal.size, 5);
  ASSERT_EQ(queue.internal.head, 0);
  ASSERT_EQ(queue.internal.tail, 5);

  queue_free(&queue);
}

TEST(TestQueue, resizeBuffer2) {
  Queue(size_t) queue;

  queue_init(&queue, 4);

  // fill up to the end
  for (size_t i = 0; i < 4; i += 1) {
    queue_enqueue(&queue, i);
  }

  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 4);
  ASSERT_EQ(queue.internal.head, 0);
  ASSERT_EQ(queue.internal.tail, 0);

  size_t got = *(size_t *)queue.internal.buffer;

  ASSERT_EQ(got, 0);

  // empty down to the end - 1 to move head to the end
  for (size_t i = 0; i < 3; i += 1) {
    size_t got = ~0;
    queue_dequeue(&queue, &got);
    ASSERT_EQ(got, i);
  }

  // fill back up and have head and tail point to the
  // same
  for (size_t i = 0; i < 3; i += 1) {
    queue_enqueue(&queue, i);
  }

  ASSERT_EQ(queue.internal.capacity, 4);
  ASSERT_EQ(queue.internal.size, 4);
  ASSERT_EQ(queue.internal.head, 3);
  ASSERT_EQ(queue.internal.tail, 3);

  // force a buffer upgrade
  size_t ff = 42;
  queue_enqueue(&queue, ff);

  ASSERT_EQ(queue.internal.capacity, 8);
  ASSERT_EQ(queue.internal.size, 5);
  ASSERT_EQ(queue.internal.head, 3);
  ASSERT_EQ(queue.internal.tail, 0);

  queue_free(&queue);
}
