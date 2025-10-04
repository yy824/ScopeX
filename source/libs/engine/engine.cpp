/*
 * @Author: lyan liuyanbc157_cn@hotmail.com
 * @Date: 2025-09-26 11:49:58
 * @LastEditors: lyan liuyanbc157_cn@hotmail.com
 * @LastEditTime: 2025-10-01 19:59:38
 * @FilePath: \scopeX1\source\libs\engine\engine.cpp
 * @Description: 
 */
#include <libs/engine/engine.hpp>
#include <algorithm>
#include <map>
#include <deque>
#include <unordered_map>

namespace engine {

// ------------Order Book Implementation---------
class OrderBook {
public:
    // only use side, price, qty, id from order
    std::vector<Trade> add_limit(Order order, TimeInForce tif, std::uint64_t timestamp);
    std::vector<Trade> add_market(Order order, TimeInForce tif, std::uint64_t timestamp, int max_levels, bool& empty_book);
    bool cancel(Id id);

    Snapshot snapshot(int depth) const;

    // for FOK (Fill-Or-Kill) check
    Qty available_to_buy_up_to(Price px) const;
    Qty available_to_sell_down_to(Price px) const;
    Qty available_market(Side side, int max_levels) const;

private:
    using BidBook = std::map<Price, std::deque<Order>, std::greater<>>; // Bid price type
    using AskBook = std::map<Price, std::deque<Order>, std::less<> >;   // Ask price type

    BidBook bids_;
    AskBook asks_;
    std::unordered_map<Id, Locate> index_; // order id -> (side, price)

    static Qty level_qty(const std::deque<Order>& q)
    {
        Qty s = 0;
        for (const auto& o : q) {
            s += o.qty;
        }
        return s;
    }

    void match_level(Order& in, std::deque<Order>& level, Price level_px, std::vector<Trade>& trades, uint64_t timestamp)
    {
        while (in.qty > 0 && !level.empty()) {
            // pick up the top order in the same price level
            Order& top = level.front();
            Qty trade_qty = std::min(in.qty, top.qty);
            trades.push_back(Trade{ in.id, top.id, level_px, trade_qty, timestamp });
            // update in order quantity. Later can be decided whether it has to be added in order list
            in.qty -= trade_qty;
            top.qty -= trade_qty;
            if (top.qty == 0) {
                index_.erase(top.id);
                level.pop_front();
            }
        }
    }
};

// ------------Order Book Implementation---------
// capacity calculation
Qty OrderBook::available_to_buy_up_to(Price px) const
{
    Qty total = 0;
    for (const auto& [ask_px, q] : asks_) {
        if (ask_px > px) break;
        total += level_qty(q);
    }
    return total;

}

Qty OrderBook::available_to_sell_down_to(Price px) const
{
    Qty total = 0;
    for (const auto& [bid_px, q] : bids_) {
        if (bid_px < px) break;
        total += level_qty(q);
    }
    return total;
}

Qty OrderBook::available_market(Side side, int max_levels) const
{
    Qty total = 0;
    int levels = 0;
    if (side == Side::BUY) {
        for (const auto& [ask_px, q] : asks_) {
            total += level_qty(q);
            if (++levels >= max_levels) break;
        }
    } else {
        for (const auto& [bid_px, q] : bids_) {
            total += level_qty(q);
            if (++levels >= max_levels) break;
        }
    }
    return total;
}

// adding limit order
std::vector<Trade> OrderBook::add_limit(Order order, TimeInForce tif, std::uint64_t timestamp)
{
    std::vector<Trade> trades;
    if (order.qty <= 0) {
        return trades; // invalid qty
    }

    if (order.side == Side::BUY) {
        // match against asks
        for (auto it = asks_.begin(); it != asks_.end() && order.qty > 0 && it->first <= order.price;) {
            match_level(order, it->second, it->first, trades, timestamp);
            if (it->second.empty()) {
                it = asks_.erase(it);
            } else {
                ++it;
            }
        }
        // remaining qty
        if (order.qty > 0) {
            if (tif == TimeInForce::GTC) {
                // add to bids and get index price level iterator
                auto [lv_it, _] = bids_.try_emplace(order.price, std::deque<Order>{});
                auto& q = lv_it->second;
                // adding into deque end at the same price level
                q.emplace_back(std::move(order));
                // index it
                auto q_it = std::prev(q.end());
                // added only for Bid. it is able to find the location for price(lv_it) then order id(q_it) with O(1)
                index_[order.id] = Locate{ .side = Side::BUY, .bid_it = lv_it, .ask_it = AskBook::iterator{}, .q_it = q_it };
            }
            // IOC/FOK unfilled portion is discarded
        }
    } else { // SELL
        // match against bids
        for (auto it = bids_.begin(); it != bids_.end() && order.qty > 0 && it->first >= order.price;) {
            match_level(order, it->second, it->first, trades, timestamp);
            if (it->second.empty()) {
                it = bids_.erase(it);
            } else {
                ++it;
            }
        }
        // remaining qty
        if (order.qty > 0) {
            if (tif == TimeInForce::GTC) {
                // add to asks and get index price level iterator
                auto [lv_it, _] = asks_.try_emplace(order.price, std::deque<Order>{});
                auto& q = lv_it->second;
                // adding into deque end at the same price level
                q.emplace_back(std::move(order));
                // index it
                auto q_it = std::prev(q.end());
                // added only for Ask. it is able to find the location for price(lv_it) then order id(q_it) with O(1)
                index_[order.id] = Locate{ .side = Side::SELL, .bid_it = BidBook::iterator{}, .ask_it = lv_it, .q_it = q_it };
            }
            // IOC/FOK unfilled portion is discarded
        }
    }
    return trades;
}

// adding market order, price is ignored here, set to 0
std::vector<Trade> OrderBook::add_market(Order order, TimeInForce tif, std::uint64_t timestamp, int max_levels, bool& empty_book)
{
    std::vector<Trade> trades;
    int level = 0;
    if (order.qty <= 0) {
        return trades; // invalid qty
    }
    if(order.side == Side::BUY)
    {
        while(order.qty > 0 && !asks_.empty())
        {
            auto it = asks_.begin();
            match_level(order, it->second, it->first, trades, timestamp);
            if(it->second.empty()) asks_.erase(it); // remove empty level
            if(max_levels > 0 && ++level >= max_levels) break; // reached max levels
        }
        empty_book = asks_.empty();
        // remaining qty is discarded for market orders
    } 
    else
    {
        while(order.qty > 0 && !bids_.empty())
        {
            auto it = bids_.begin();
            match_level(order, it->second, it->first, trades, timestamp);
            if(it->second.empty()) bids_.erase(it); // remove empty level
            if(max_levels > 0 && ++level >= max_levels) break; // reached max levels
        }
        empty_book = bids_.empty();
        // remaining qty is discarded for market orders
    }
    return trades;
}

bool OrderBook::cancel(Id id)
{
    auto it = index_.find(id);
    if (it == index_.end()) {
        return false; // not found
    }
    
    // with O(1) cancel function it is much faster than O(logN) search + O(1) erase
    auto& loc = it->second; // get locate info
    if (loc.side == Side::BUY) {
        // find price level
        auto& q = loc.bid_it->second;
        // erase order in the price level queue
        q.erase(loc.q_it); // erase the queue iterator from deque
        if (q.empty()) {
            bids_.erase(loc.bid_it);
        } // remove empty price level
    }
    else
    {
        // find price level
        auto& q = loc.ask_it->second;
        // erase order in the price level queue
        q.erase(loc.q_it); // erase the queue iterator from deque
        if (q.empty()) {
            asks_.erase(loc.ask_it);
        } // remove empty price level
    }
    index_.erase(it); // remove from index
    return true;
}

Snapshot OrderBook::snapshot(int depth) const
{
    Snapshot snap;
    snap.bids.reserve(depth > 0 ? depth : 10);
    snap.asks.reserve(depth > 0 ? depth : 10);

    auto bit = bids_.begin();
    auto ait = asks_.begin();

    for(int i = 0; i < depth; i++)
    {
        if(bit != bids_.end())
        {
            snap.bids.push_back(SnapshotLevel{bit->first, level_qty(bit->second)});
            ++bit;
        }
        if(ait != asks_.end())
        {
            snap.asks.push_back(SnapshotLevel{ait->first, level_qty(ait->second)});
            ++ait;
        }
    }

    return snap;
}


// ------------Engine Implementation---------
// V1: simple single thread implementation
// stop for further derivation. For safe capsulation, make it final.
class EngineSingleThreaded final: public IEngine {
public:
    explicit EngineSingleThreaded(const EngineConfig& config): config_(config) {}
    AddResult addOrder(const OrderCmd& cmd) override;
    bool cancelOrder(Id orderId) override { return ob_.cancel(orderId); };
    Snapshot snapshot(int depth = 5) const override {return ob_.snapshot(depth);};
  

private:
    EngineConfig config_;
    OrderBook ob_;
    Id next_{1000};
    uint64_t seq_{0}; // internal sequence number for ordering
};

AddResult EngineSingleThreaded::addOrder(const OrderCmd& cmd)
{
    if (cmd.qty <= 0) 
    {
        return AddResult{ OrderStatus::BAD_INPUT, 0, {}, 0, 0 };
    }

    // determine order id by user provided or internal, only if orderId is not set, use internal next_ and increment it.
    Id order_id = cmd.orderId.value_or(next_++);

    uint64_t timestamp = ++seq_; // internal sequence number for ordering -> in the future can be replaced by global time source

    std::vector<Trade> trades;
    OrderStatus status = OrderStatus::OK;
    Qty filled_qty = 0, remaining_qty = 0;

    if(cmd.orderType==OrderType::LIMIT)
    {
        // FOK (Fill-Or-Kill) check
        if(cmd.timeInForce == TimeInForce::FOK)
        {
            const bool ok = (cmd.side == Side::BUY) ? 
                (ob_.available_to_buy_up_to(cmd.price) >= cmd.qty) :
                (ob_.available_to_sell_down_to(cmd.price) >= cmd.qty);
            if(!ok)
            {
                return AddResult{ OrderStatus::FOK_FAIL, order_id, {}, 0, cmd.qty};
            }
        }

        trades = ob_.add_limit(Order{cmd.orderId.value_or(order_id), cmd.side, cmd.price, cmd.qty, 0}, cmd.timeInForce, timestamp);
        for(auto& t:trades) filled_qty += t.qty;
        remaining_qty = cmd.qty - filled_qty;

        status = (filled_qty == 0 ? OrderStatus::OK :
                  (remaining_qty == 0 ? OrderStatus::FILLED : OrderStatus::PARTIAL));
    } else { // MARKET
        if(cmd.timeInForce == TimeInForce::FOK)
        {
            const auto available = ob_.available_market(cmd.side, config_.market_max_levels);
            if(available < cmd.qty)
            {
                return AddResult{ OrderStatus::FOK_FAIL, order_id, {}, 0, cmd.qty};
            }
        } 
        if(cmd.timeInForce == TimeInForce::GTC && !config_.market_gtc_as_ioc)
        {
            return AddResult{ OrderStatus::REJECT, order_id, {}, 0, cmd.qty};
        }

        bool empty_book = false;
        trades = ob_.add_market(Order{cmd.orderId.value_or(order_id), cmd.side, 0, cmd.qty, 0}, 
                                cmd.timeInForce, timestamp, config_.market_max_levels, empty_book);

        for(auto& t:trades) filled_qty += t.qty;
        remaining_qty = cmd.qty - filled_qty;

        if(filled_qty == 0 && empty_book)
        {
            status = OrderStatus::EMPTY_BOOK;
        }
        else
        {
            status = (filled_qty == 0 ? OrderStatus::OK :
                      (remaining_qty == 0 ? OrderStatus::FILLED : OrderStatus::PARTIAL));
        }
    }
    return AddResult{ status, order_id, trades, filled_qty, remaining_qty};
}

/// create a unique pointer of engine
/// for future extension, can create different engine implementations based on config
std::unique_ptr<IEngine> make_engine(const EngineConfig& config)
{
    return std::make_unique<EngineSingleThreaded>(config);
}
}