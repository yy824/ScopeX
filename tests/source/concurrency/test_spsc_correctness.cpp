#include <gtest/gtest.h>
#include "libs/concurrency/spsc_ring.hpp"
#include <thread>
#include <atomic>
#include <vector>

using concurrency::SpscRing;

TEST(SpscRing, SingleThread_Order) {
  constexpr std::size_t CAP = 1u << 8; // 256
  SpscRing<int> q(CAP);

  // 前 CAP 次 push 必须成功
  for (std::size_t i = 0; i < CAP; ++i) {
    EXPECT_TRUE(q.push(static_cast<int>(i))) << "i=" << i;
  }

  // 第 CAP+1 次应该失败（已满）
  EXPECT_FALSE(q.push(123)) << "should fail when full";

  // 全部弹出，计数 = CAP，顺序一致
  int v = -1;
  int expect = 0;
  std::size_t cnt = 0;
  while (q.pop(v)) {
    EXPECT_EQ(v, expect++);
    ++cnt;
  }
  EXPECT_EQ(cnt, CAP);
}

TEST(SpscRing, TwoThreads_Order) {
  constexpr int N = 200000;
  SpscRing<int> q(1u << 15);

  std::atomic<bool> go{false};
  std::thread prod([&]{
    while (!go.load(std::memory_order_acquire)) {}
    for (int i=0;i<N;) { if (q.push(i)) ++i; else std::this_thread::yield(); }
  });

  std::thread cons([&]{
    while (!go.load(std::memory_order_acquire)) {}
    int expect=0, v, got=0;
    while (got<N) {
      if (q.pop(v)) { ASSERT_EQ(v, expect++); ++got; }
      else std::this_thread::yield();
    }
  });

  go.store(true, std::memory_order_release);
  prod.join(); cons.join();
}

// wrap-around：连续循环越过多次容量边界
TEST(SpscRing, WrapAround_LongRun) {
  constexpr int N = 2'000'000;    // 至少多次越界
  SpscRing<int> q(1u << 12);      // 小容量，强制频繁 wrap
  std::atomic<bool> go{false};

  std::thread prod([&]{
    while (!go.load(std::memory_order_acquire)) {}
    for (int i=0;i<N;) { if (q.push(i)) ++i; else std::this_thread::yield(); }
  });
  std::thread cons([&]{
    while (!go.load(std::memory_order_acquire)) {}
    int expect=0, v, got=0;
    while (got<N) {
      size_t burst = 0;
      while (burst < 128 && q.pop(v)) {
        ASSERT_EQ(v, expect++);
        ++got; ++burst;
      }
      if (burst==0) std::this_thread::yield();
    }
  });

  go.store(true, std::memory_order_release);
  prod.join(); cons.join();
}