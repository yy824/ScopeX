#include <gtest/gtest.h>
#include "libs/concurrency/spsc_ring.hpp"

using concurrency::SpscRing;

TEST(SpscRing, EmptyPop) {
  SpscRing<int> q(1u << 10);
  int v=0;
  EXPECT_FALSE(q.pop(v));
}

TEST(SpscRing, FullPush) {
  constexpr std::size_t CAP = 1u << 4; // 16
  SpscRing<int> q(CAP);

  for (std::size_t i = 0; i < CAP; ++i) {
    EXPECT_TRUE(q.push(static_cast<int>(i))) << "i=" << i;
  }
  EXPECT_FALSE(q.push(999)) << "push should fail when full";
}

// 如果你在构造里有 assert(cap 是 2 的幂)，可以写一条 death test（可选）
// TEST(SpscRing, CapacityPowerOfTwo) {
//   ASSERT_DEATH( SpscRing<int>(1000), "capacity must be power of two" );
// }