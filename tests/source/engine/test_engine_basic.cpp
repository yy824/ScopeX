#include <gtest/gtest.h>
#include <libs/engine/engine.hpp>

using namespace engine;

TEST(EngineBasic, CrossLimitAndSnapshot) {
  auto eng = make_engine({true, 0});
  eng->addOrder({.side=Side::SELL,.orderType=OrderType::LIMIT,.timeInForce=TimeInForce::GTC,.price=10050,.qty=7});
  eng->addOrder({.side=Side::SELL,.orderType=OrderType::LIMIT,.timeInForce=TimeInForce::GTC,.price=10100,.qty=5});
  eng->addOrder({.side=Side::BUY ,.orderType=OrderType::LIMIT,.timeInForce=TimeInForce::GTC,.price= 9950,.qty=10});

  auto r = eng->addOrder({.side=Side::BUY,.orderType=OrderType::LIMIT,.timeInForce=TimeInForce::GTC,.price=10100,.qty=12});
  long long filled = 0; for (auto& t : r.trades) filled += t.qty;
  EXPECT_EQ(filled, 12);
  EXPECT_EQ(r.status, OrderStatus::FILLED);

  auto s = eng->snapshot(1);
  ASSERT_GE(s.bids.size(), 1);
  EXPECT_EQ(s.bids[0].price, 9950);
  EXPECT_EQ(s.bids[0].qty,   10);
  EXPECT_TRUE(s.asks.empty());
}