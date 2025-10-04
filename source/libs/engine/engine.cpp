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

// ------------order_t Book Implementation---------
class OrderBook {
public:
    // only use side, price, qty, id from order
    std::vector<trade_t> add_limit(order_t order, TimeInForce tif, std::uint64_t timestamp);
    std::vector<trade_t> add_market(order_t order, std::uint64_t timestamp, std::uint16_t max_levels, bool& empty_book);
    bool cancel(id_t order_id);

    snapshot_t snapshot(int depth) const;

    // for FOK (Fill-Or-Kill) check
    qty_t available_to_buy_up_to(price_t price) const;
    qty_t available_to_sell_down_to(price_t price) const;
    qty_t available_market(Side side, std::uint16_t max_levels) const;

private:
    using BidBook = std::map<price_t, std::deque<order_t>, std::greater<>>; // Bid price type
    using AskBook = std::map<price_t, std::deque<order_t>, std::less<> >;   // Ask price type

    BidBook bids_;
    AskBook asks_;
    std::unordered_map<id_t, locate_t> index_; // order id -> (side, price)

    static qty_t level_qty(const std::deque<order_t>& price_level)
    {
        qty_t quantity = 0;
        for (const auto& order : price_level) {
            quantity += order.qty;
        }
        return quantity;
    }

    void match_level(order_t& in_order, std::deque<order_t>& level, price_t level_px, std::vector<trade_t>& trades, uint64_t timestamp)
    {
        while (in_order.qty > 0 && !level.empty()) {
            // pick up the top order in the same price level
            order_t& top = level.front();
            qty_t trade_qty = std::min(in_order.qty, top.qty);
            trades.push_back( trade_t{ .taker=in_order.id, .maker=top.id, .price=level_px, .qty=trade_qty, .timestamp=timestamp } );
            // update in order quantity. Later can be decided whether it has to be added in order list
            in_order.qty -= trade_qty;
            top.qty -= trade_qty;
            if (top.qty == 0) {
                index_.erase(top.id);
                level.pop_front();
            }
        }
    }
};

// ------------order_t Book Implementation---------
// capacity calculation
qty_t OrderBook::available_to_buy_up_to(price_t price) const
{
    qty_t total = 0;
    for (const auto& [ask_px, price_level] : asks_) {
        if (ask_px > price) {break;}
        total += level_qty(price_level);
    }
    return total;

}

qty_t OrderBook::available_to_sell_down_to(price_t price) const
{
    qty_t total = 0;
    for (const auto& [bid_px, price_level] : bids_) {
        if (bid_px < price) {break;}
        total += level_qty(price_level);
    }
    return total;
}

qty_t OrderBook::available_market(Side side, std::uint16_t max_levels) const
{
    qty_t total = 0;
    std::uint16_t levels = 0;
    if (side == Side::BUY) {
        for (const auto& [ask_px, order_queue] : asks_) {
            total += level_qty(order_queue);
            if (++levels >= max_levels) {break;}
        }
    } else {
        for (const auto& [bid_px, order_queue] : bids_) {
            total += level_qty(order_queue);
            if (++levels >= max_levels) {break;}
        }
    }
    return total;
}

// adding limit order
std::vector<trade_t> OrderBook::add_limit(order_t order, TimeInForce tif, std::uint64_t timestamp)
{
    std::vector<trade_t> trades;
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
                auto [lv_it, _unused_bool] = bids_.try_emplace(order.price, std::deque<order_t>{});
                auto& order_queue = lv_it->second;
                // adding into deque end at the same price level
                order_queue.emplace_back(order);
                // index it
                auto q_it = std::prev(order_queue.end());
                // added only for Bid. it is able to find the location for price(lv_it) then order id(q_it) with O(1)
                index_[order.id] = locate_t{ .side = Side::BUY, .bid_it = lv_it, .ask_it = AskBook::iterator{}, .q_it = q_it };
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
                auto [lv_it, _unused_bool] = asks_.try_emplace(order.price, std::deque<order_t>{});
                auto& order_queue = lv_it->second;
                // adding into deque end at the same price level
                order_queue.emplace_back(order);
                // index it
                auto q_it = std::prev(order_queue.end());
                // added only for Ask. it is able to find the location for price(lv_it) then order id(q_it) with O(1)
                index_[order.id] = locate_t{ .side = Side::SELL, .bid_it = BidBook::iterator{}, .ask_it = lv_it, .q_it = q_it };
            }
            // IOC/FOK unfilled portion is discarded
        }
    }
    return trades;
}

// matching only, remaining qty is discarded
std::vector<trade_t> OrderBook::add_market(order_t order, std::uint64_t timestamp, std::uint16_t max_levels, bool& empty_book)
{
    if (order.qty <= 0) {
        return {}; // invalid qty
    }
    std::vector<trade_t> trades;
    std::uint16_t level = 0;

    if(order.side == Side::BUY)
    {
        while(order.qty > 0 && !asks_.empty())
        {
            auto ask_it = asks_.begin();
            match_level(order, ask_it->second, ask_it->first, trades, timestamp);
            if(ask_it->second.empty()) {asks_.erase(ask_it);} // remove empty level
            if(max_levels > 0 && ++level >= max_levels) {break;} // reached max levels
        }
        empty_book = asks_.empty();
        // remaining qty is discarded for market orders
    } 
    else
    {
        while(order.qty > 0 && !bids_.empty())
        {
            auto bid_it = bids_.begin();
            match_level(order, bid_it->second, bid_it->first, trades, timestamp);
            if(bid_it->second.empty()) {bids_.erase(bid_it);} // remove empty level
            if(max_levels > 0 && ++level >= max_levels) {break;} // reached max levels
        }
        empty_book = bids_.empty();
        // remaining qty is discarded for market orders
    }
    return trades;
}

bool OrderBook::cancel(id_t order_id)
{
    auto price_it = index_.find(order_id);
    if (price_it == index_.end()) {
        return false; // not found
    }
    
    // with O(1) cancel function it is much faster than O(logN) search + O(1) erase
    auto& loc = price_it->second; // get locate info
    if (loc.side == Side::BUY) {
        // find price level
        auto& order_queue = loc.bid_it->second;
        // erase order in the price level queue
        order_queue.erase(loc.q_it); // erase the queue iterator from deque
        if (order_queue.empty()) {
            bids_.erase(loc.bid_it);
        } // remove empty price level
    }
    else
    {
        // find price level
        auto& order_queue = loc.ask_it->second;
        // erase order in the price level queue
        order_queue.erase(loc.q_it); // erase the queue iterator from deque
        if (order_queue.empty()) {
            asks_.erase(loc.ask_it);
        } // remove empty price level
    }
    index_.erase(price_it); // remove from index
    return true;
}

snapshot_t OrderBook::snapshot(int depth) const
{
    snapshot_t snap;
    snap.bids.reserve(depth > 0 ? depth : 10);
    snap.asks.reserve(depth > 0 ? depth : 10);

    auto bit = bids_.begin();
    auto ait = asks_.begin();

    for(int i = 0; i < depth; i++)
    {
        if(bit != bids_.end())
        {
            snap.bids.push_back(snapshot_level_t{bit->first, level_qty(bit->second)});
            ++bit;
        }
        if(ait != asks_.end())
        {
            snap.asks.push_back(snapshot_level_t{ait->first, level_qty(ait->second)});
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
    explicit EngineSingleThreaded(const engine_config_t& config): config_(config) {}
    add_result_t add_order(const order_cmd_t& cmd) override;
    bool cancel_order(id_t order_id) override { return ob_.cancel(order_id); };
    snapshot_t snapshot(int depth) const override {return ob_.snapshot(depth);};
  

private:
    engine_config_t config_;
    OrderBook ob_;
    id_t next_{1000};
    uint64_t seq_{0}; // internal sequence number for ordering
};

add_result_t EngineSingleThreaded::add_order(const order_cmd_t& cmd)
{
    // 0. basic validation
    if (cmd.qty <= 0) 
    {
        return add_result_t{ .status=OrderStatus::BAD_INPUT, .order_id=0, .trades={}, .filled_qty=0, .remaining_qty=cmd.qty };
    }

    if (cmd.order_type == OrderType::LIMIT && cmd.price <= 0) 
    {
        return add_result_t{ .status=OrderStatus::BAD_INPUT, .order_id=0, .trades={}, .filled_qty=0, .remaining_qty=cmd.qty };
    }

    // 1. assign a new order id if not provided.
    id_t order_id = cmd.order_id.value_or(next_++);

    uint64_t timestamp = ++seq_; // internal sequence number for ordering -> in the future can be replaced by global time source

    std::vector<trade_t> trades;
    OrderStatus status = OrderStatus::OK;
    qty_t filled_qty = 0;
    qty_t remaining_qty = 0;

    if(cmd.order_type==OrderType::LIMIT)
    {
        // FOK (Fill-Or-Kill) check
        if(cmd.time_in_force == TimeInForce::FOK)
        {
            const bool is_ok = (cmd.side == Side::BUY) ? 
                (ob_.available_to_buy_up_to(cmd.price) >= cmd.qty) :
                (ob_.available_to_sell_down_to(cmd.price) >= cmd.qty);
            if(!is_ok)
            {
                return add_result_t{ .status=OrderStatus::FOK_FAIL, .order_id=order_id, .trades={}, .filled_qty=0, .remaining_qty=cmd.qty};
            }
        }

        // implement limit orders
        trades = ob_.add_limit(order_t{.id=cmd.order_id.value_or(order_id), .side=cmd.side, .price=cmd.price, .qty=cmd.qty, .seq_num=0}, cmd.time_in_force, timestamp);
        for(const auto& trade:trades) {filled_qty += trade.qty;}
        remaining_qty = cmd.qty - filled_qty;

        // State machine
        if(cmd.time_in_force == TimeInForce::FOK)
        {
            status = (remaining_qty == 0 ? OrderStatus::FILLED : OrderStatus::FOK_FAIL);
        }
        else if(cmd.time_in_force == TimeInForce::IOC)
        {
            status = (filled_qty == 0 ? OrderStatus::OK :
                      (remaining_qty == 0 ? OrderStatus::FILLED : OrderStatus::PARTIAL));
        }
        else // GTC
        {
            status = (filled_qty == 0 ? OrderStatus::OK :
                      (remaining_qty == 0 ? OrderStatus::FILLED : OrderStatus::OK));
        }
    } 
    else // MARKET
    { 
        // MARKET order validation
        if(cmd.time_in_force == TimeInForce::FOK)
        {
            const auto available = ob_.available_market(cmd.side, config_.market_max_levels);
            if(available < cmd.qty)
            {
                return add_result_t{ .status=OrderStatus::FOK_FAIL, .order_id=order_id, .trades={}, .filled_qty=0, .remaining_qty=cmd.qty};
            }
        } 
        
        // MARKET + GTC
        if(cmd.time_in_force == TimeInForce::GTC && !config_.market_gtc_as_ioc)
        {
            return add_result_t{ .status=OrderStatus::REJECT, .order_id=order_id, .trades={}, .filled_qty=0, .remaining_qty=cmd.qty};
        }

        bool empty_book = false;
        trades = ob_.add_market(order_t{.id=cmd.order_id.value_or(order_id), .side=cmd.side, .price=0, .qty=cmd.qty, .seq_num=0}, 
                                timestamp, config_.market_max_levels, empty_book);

        for(const auto& trade:trades) {filled_qty += trade.qty;}
        remaining_qty = cmd.qty - filled_qty;

        // State machine
        if(filled_qty == 0 && empty_book)
        {
            status = OrderStatus::EMPTY_BOOK; // no liquidity at all
        }
        else if(cmd.time_in_force == TimeInForce::FOK)
        {
            status = (remaining_qty == 0 ? OrderStatus::FILLED : OrderStatus::FOK_FAIL);
        }
        else // IOC or GTC(as IOC)
        {
            status = (remaining_qty == 0 ? OrderStatus::FILLED :
                      (filled_qty > 0 ? OrderStatus::PARTIAL : OrderStatus::OK));
        }
    }
    return add_result_t{ .status=status, .order_id=order_id, .trades=std::move(trades), .filled_qty=filled_qty, .remaining_qty=remaining_qty};
}

/// create a unique pointer of engine
/// for future extension, can create different engine implementations based on config
std::unique_ptr<IEngine> make_engine(const engine_config_t& config)
{
    return std::make_unique<EngineSingleThreaded>(config);
}

} // namespace engine