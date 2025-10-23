#include "libs/concurrency/spsc_ring.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>

using concurrency::SpscRing;
using namespace std::chrono;

static void die(const char* msg) {
  std::fprintf(stderr, "[FAIL] %s\n", msg);
  std::fflush(stderr);
  std::exit(1);
}
static void ok(const char* name) {
  std::printf("[OK] %s\n", name);
}

static void test_single_thread_basic() {
  SpscRing<int> q(1u << 8); // 256
  // 装满 cap-1 个（避免 head==tail 二义性）
  for (int i=0;i<256;++i) if (!q.push(i)) die("push failed before full");
  if (q.push(999)) die("push should fail when full");
  int v=0, cnt=0;
  while (q.pop(v)) {
    if (v != cnt) die("order mismatch (single thread)");
    ++cnt;
  }
  if (cnt != 256) die("pop count mismatch (single thread)");
  ok("single_thread_basic");
}

// 双线程顺序 + 计数校验（用总和与平方和双校验，防止调包/重复）
static void test_two_threads_order() {
  constexpr int N = 200'000;
  SpscRing<int> q(1u << 15);
  std::atomic<bool> start{false};

  std::thread prod([&]{
    while (!start.load(std::memory_order_acquire)) {}
    long long sum=0, sq=0;
    for (int i=0;i<N; ) {
      if (q.push(i)) { sum += i; sq += 1LL*i*i; ++i; }
      else std::this_thread::yield();
    }
    std::printf("[prod] sum=%lld sq=%lld\n", sum, sq);
  });

  std::thread cons([&]{
    while (!start.load(std::memory_order_acquire)) {}
    int got=0, v=0, expect=0;
    long long sum=0, sq=0;
    while (got < N) {
      if (q.pop(v)) {
        if (v != expect) die("order mismatch (two threads)");
        sum += v; sq += 1LL*v*v;
        ++expect; ++got;
      } else {
        std::this_thread::yield();
      }
    }
    // 理论值：0..N-1 的和与平方和
    long long th_sum = 1LL*(N-1)*N/2;
    long long th_sq  = 1LL*(N-1)*N*(2LL*N-1)/6;
    if (sum!=th_sum || sq!=th_sq) die("checksum mismatch (two threads)");
    std::printf("[cons] sum=%lld sq=%lld (expected)\n", th_sum, th_sq);
  });

  start.store(true, std::memory_order_release);
  prod.join(); cons.join();
  ok("two_threads_order");
}

// 小容量强制频繁 wrap-around
static void test_wraparound_longrun() {
  constexpr int N = 2'000'000;
  SpscRing<int> q(1u << 10); // 1024
  std::atomic<bool> start{false};

  std::thread prod([&]{
    while (!start.load(std::memory_order_acquire)) {}
    for (int i=0;i<N; ) {
      if (q.push(i)) ++i; else std::this_thread::yield();
    }
  });
  std::thread cons([&]{
    while (!start.load(std::memory_order_acquire)) {}
    int expect=0, v=0, got=0;
    while (got<N) {
      size_t burst=0;
      while (burst<128 && q.pop(v)) {
        if (v != expect) die("order mismatch (wraparound)");
        ++expect; ++got; ++burst;
      }
      if (burst==0) std::this_thread::yield();
    }
  });

  start.store(true, std::memory_order_release);
  prod.join(); cons.join();
  ok("wraparound_longrun");
}

// 批量 try_pop_n 验证顺序一致性
static void test_batch_try_pop_n() {
  constexpr int N = 100'000;
  SpscRing<int> q(1u << 15);
  std::atomic<bool> start{false};

  std::thread prod([&]{
    while (!start.load(std::memory_order_acquire)) {}
    for (int i=0;i<N; ) {
      if (q.push(i)) ++i; else std::this_thread::yield();
    }
  });
  std::thread cons([&]{
    while (!start.load(std::memory_order_acquire)) {}
    std::vector<int> buf(512);
    int expect=0, got=0;
    while (got<N) {
      auto n = q.try_pop_n(buf.data(), buf.size());
      for (size_t i=0;i<n;++i) {
        if (buf[i] != expect) die("order mismatch (batch)");
        ++expect; ++got;
      }
      if (n==0) std::this_thread::yield();
    }
  });

  start.store(true, std::memory_order_release);
  prod.join(); cons.join();
  ok("batch_try_pop_n");
}

// 随机节奏压力
static void test_stress_random() {
  constexpr int N = 1'000'000;
  SpscRing<int> q(1u << 14);
  std::atomic<bool> start{false};

  std::thread prod([&]{
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> d(0, 10);
    while (!start.load(std::memory_order_acquire)) {}
    for (int i=0;i<N; ) {
      if (q.push(i)) { ++i; }
      else { std::this_thread::yield(); }
      if (d(rng)==0) std::this_thread::yield();
    }
  });
  std::thread cons([&]{
    std::mt19937 rng(456);
    std::uniform_int_distribution<int> d(0, 20);
    while (!start.load(std::memory_order_acquire)) {}
    int v=0, expect=0, got=0;
    while (got<N) {
      if (q.pop(v)) {
        if (v!=expect) die("order mismatch (stress)");
        ++expect; ++got;
      } else {
        std::this_thread::yield();
      }
      if (d(rng)==0) std::this_thread::yield();
    }
  });

  start.store(true, std::memory_order_release);
  prod.join(); cons.join();
  ok("stress_random");
}

int main() {
  auto t0 = steady_clock::now();
  test_single_thread_basic();
  test_two_threads_order();
  test_wraparound_longrun();
  test_batch_try_pop_n();
  test_stress_random();
  auto t1 = steady_clock::now();
  auto ms = duration_cast<milliseconds>(t1 - t0).count();
  std::printf("ALL TESTS PASS in %lld ms\n", (long long)ms);
  return 0;
}