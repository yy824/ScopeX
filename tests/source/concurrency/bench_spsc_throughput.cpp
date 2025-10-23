#include "libs/concurrency/spsc_ring.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdio>

using namespace std::chrono;
using concurrency::SpscRing;

int main() {
  constexpr int N = 5'000'000;
  SpscRing<int> q(1u << 16);
  std::atomic<bool> go{false};

  auto t0 = steady_clock::now();
  std::thread prod([&]{
    while (!go.load(std::memory_order_acquire)) {}
    for (int i=0;i<N;) { if (q.push(i)) ++i; else std::this_thread::yield(); }
  });

  std::thread cons([&]{
    while (!go.load(std::memory_order_acquire)) {}
    int v, got=0;
    auto start = steady_clock::now();
    while (got<N) {
      if (q.pop(v)) ++got; else std::this_thread::yield();
    }
    auto end = steady_clock::now();
    auto dt = duration_cast<milliseconds>(end - start).count();
    double qps = (double)N / (dt / 1000.0);
    std::printf("handled=%d  time=%lld ms  throughput=%.1f ops/s\n", N, (long long)dt, qps);
  });

  go.store(true, std::memory_order_release);
  prod.join(); cons.join();
  return 0;
}