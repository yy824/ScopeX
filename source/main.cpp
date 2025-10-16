/*
 * @Author: lyan liuyanbc157_cn@hotmail.com
 * @Date: 2025-09-26 11:49:58
 * @LastEditors: lyan liuyanbc157_cn@hotmail.com
 * @LastEditTime: 2025-10-01 19:46:51
 * @FilePath: \scopeX1\source\main.cpp
 * @Description: 
 */
#include <iostream>
#include <iomanip>
#include <string>

#include <libs/engine/engine.hpp>

using namespace engine;

static void print_trades(const std::vector<trade_t>& trades)
{
  using std::cout;
  using std::fixed;
  using std::setprecision;

  for(const auto& trade:trades)
  {
    cout << "TRADE taker=" << trade.taker 
    << " maker=" << trade.maker 
    << " price=" << fixed << setprecision(2) << trade.price/100.0
    << " quantity=" << trade.qty
    << '\n';
  }
}

static void print_snapshot(const snapshot_t& snap)
{
  using std::cout;
  using std::left;
  using std::setw;
  using std::fixed;
  using std::setprecision;

  cout << "===== Order Book snapshot_t (top) =====\n";
  cout << left << setw(20) << "BIDS" << "| " << left << setw(20) << "ASKS" << "\n";

  auto it_ask = snap.asks.rbegin();
  auto it_bid = snap.bids.rbegin();
  while(it_ask != snap.asks.rend() || it_bid != snap.bids.rend())
  {
    cout << left << setw(20) << (it_bid != snap.bids.rend() ? "price=" + std::to_string(it_bid->price/100.0) + " qty=" + std::to_string(it_bid->qty) : "")
         << "| " << left << setw(20) << (it_ask != snap.asks.rend() ? "price=" + std::to_string(it_ask->price/100.0) + " qty=" + std::to_string(it_ask->qty) : "")
         << "\n";
    if(it_ask != snap.asks.rend()) ++it_ask;
    if(it_bid != snap.bids.rend()) ++it_bid;
  }
  cout << "=====================================\n";
}

auto main() -> int
{
  auto engine = make_engine({/*market_gtc_as_ioc*/true, /*markets_max_levels*/0});

  // Seed
  engine->add_order({.side=Side::SELL, .order_type=OrderType::LIMIT, .time_in_force=TimeInForce::GTC, .price=10100, .qty=7});
  engine->add_order({.side=Side::SELL, .order_type=OrderType::LIMIT, .time_in_force=TimeInForce::GTC, .price=10200, .qty=5});
  engine->add_order({.side=Side::BUY, .order_type=OrderType::LIMIT, .time_in_force=TimeInForce::GTC, .price=9500, .qty=10});

  print_snapshot(engine->snapshot(3));

  // cross limit
  auto r1 = engine->add_order({.side=Side::BUY, .order_type=OrderType::LIMIT, .time_in_force=TimeInForce::GTC, .price=10200, .qty=13});
  print_trades(r1.trades);
  print_snapshot(engine->snapshot(3));

  // market ioc
  auto r2 = engine->add_order({.side=Side::SELL, .order_type=OrderType::MARKET, .time_in_force=TimeInForce::IOC, .qty=8});

  print_trades(r2.trades);
  print_snapshot(engine->snapshot(3));
  return 0;
}


