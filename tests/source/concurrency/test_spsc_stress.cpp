#include <gtest/gtest.h>
#include "libs/concurrency/spsc_ring.hpp"
#include <thread>
#include <random>
#include <atomic>
#include <chrono>

using concurrency::SpscRing;

TEST(SpscRing, Stress_RandomCadence) {
  constexpr int N = 1'000'000;
  SpscRing<int> q(1u << 14);
  std::atomic<bool> go{false};

  std::thread prod([&]{
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> dist(0, 10);
    while (!go.load(std::memory_order_acquire)) {}
    for (int i=0;i<N;) {
      if (q.push(i)) { ++i; }
      else           { std::this_thread::yield(); }
      if (dist(rng)==0) std::this_thread::yield(); // 随机打乱节奏
    }
  });

  std::thread cons([&]{
    std::mt19937 rng(456);
    std::uniform_int_distribution<int> dist(0, 20);
    while (!go.load(std::memory_order_acquire)) {}
    int expect=0, v, got=0;
    while (got<N) {
      if (q.pop(v)) { ASSERT_EQ(v, expect++); ++got; }
      else          { std::this_thread::yield(); }
      if (dist(rng)==0) std::this_thread::yield();
    }
  });

  go.store(true, std::memory_order_release);
  prod.join(); cons.join();
}