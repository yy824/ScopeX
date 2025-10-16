#include <gtest/gtest.h>
#include <libs/engine/engine.hpp>

using namespace engine;

TEST(EngineBasic, CrossLimitAndSnapshot) {
  auto eng = make_engine({true, 0});
  eng->add_order({.side=Side::SELL,.order_type=OrderType::LIMIT,.time_in_force=TimeInForce::GTC,.price=10050,.qty=7});
  eng->add_order({.side=Side::SELL,.order_type=OrderType::LIMIT,.time_in_force=TimeInForce::GTC,.price=10100,.qty=5});
  eng->add_order({.side=Side::BUY ,.order_type=OrderType::LIMIT,.time_in_force=TimeInForce::GTC,.price= 9950,.qty=10});

  auto r = eng->add_order({.side=Side::BUY,.order_type=OrderType::LIMIT,.time_in_force=TimeInForce::GTC,.price=10100,.qty=12});
  long long filled = 0; for (auto& t : r.trades) filled += t.qty;
  EXPECT_EQ(filled, 12);
  EXPECT_EQ(r.status, OrderStatus::FILLED);

  auto s = eng->snapshot(1);
  ASSERT_GE(s.bids.size(), 1);
  EXPECT_EQ(s.bids[0].price, 9950);
  EXPECT_EQ(s.bids[0].qty,   10);
  EXPECT_TRUE(s.asks.empty());
}

TEST(CancelO1, CancelHead) {
  auto eng = make_engine({true, 0});
  eng->add_order({.side=Side::BUY, .order_type=OrderType::LIMIT, .time_in_force=TimeInForce::GTC, .price=100, .qty=10});
  eng->add_order({.side=Side::BUY, .order_type=OrderType::LIMIT, .time_in_force=TimeInForce::GTC, .price=100, .qty=20});
    ASSERT_TRUE(eng->cancel_order(1000));
    auto snap = eng->snapshot(5);
    EXPECT_EQ(snap.bids[0].qty, 20);
}