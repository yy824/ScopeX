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
#include <future>
#include <map>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include "libs/concurrency/spsc_ring.hpp"
#include <thread>
#include <shared_mutex>
#include <atomic>

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

// common part of single thread and asynchronous
struct single_thread_sync_t{
    template<class F> decltype(auto) write(F&& func) {return func();}
    template<class F> decltype(auto) read(F&& func) {return func();}
};

struct shared_mutex_sync_t{
    std::shared_mutex& mtx;
    template<class F> decltype(auto) write(F&& func) {std::unique_lock locker(mtx); return func();}
    template<class F> decltype(auto) read(F&& func) {std::shared_lock locker(mtx); return func();}
};


template<class Sync>
add_result_t add_impl(Sync& sync, const engine_config_t& config_, OrderBook& ob_, engine_metrics_t& metrics_, id_t& next_, uint64_t& seq_, const order_cmd_t& cmd)
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

    // Measurement variables
    const auto t_start = std::chrono::high_resolution_clock::now();

    // logic variables
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
        sync.write([&]{trades = ob_.add_limit(order_t{.id=cmd.order_id.value_or(order_id), 
            .side=cmd.side, .price=cmd.price, .qty=cmd.qty, .seq_num=0}, cmd.time_in_force, timestamp);});

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
        sync.write([&]{trades = ob_.add_market(order_t{.id=cmd.order_id.value_or(order_id), 
            .side=cmd.side, .price=0, .qty=cmd.qty, .seq_num=0}, 
            timestamp, config_.market_max_levels, empty_book);});
        

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

    // timing calculation
    const auto t_end = std::chrono::high_resolution_clock::now();
    const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start);

    // write metrics -> atomic like, get mutex, nothing else is able to write
    sync.write([&]{
        metrics_.add_orders++;
        metrics_.trades += trades.size();
        for(auto& trade: trades) { metrics_.traded_qty += trade.qty; }

        metrics_.add_total_ns += duration_ns.count();
        // find min and max latency
        metrics_.add_min_ns = std::min(metrics_.add_min_ns, static_cast<uint64_t>(duration_ns.count()));
        metrics_.add_max_ns = std::max(metrics_.add_max_ns, static_cast<uint64_t>(duration_ns.count()));

        // refresh best bid/ask (O(1) speed)
        auto snapshot_moment = ob_.snapshot(1);

        if(!snapshot_moment.bids.empty())
        {
            metrics_.best_bid_px = snapshot_moment.bids[0].price;
            metrics_.best_bid_qty = snapshot_moment.bids[0].qty;
        }
        else
        {
            metrics_.best_bid_px = 0;
            metrics_.best_bid_qty = 0;
        }

        if(!snapshot_moment.asks.empty())
        {
            metrics_.best_ask_px = snapshot_moment.asks[0].price;
            metrics_.best_ask_qty = snapshot_moment.asks[0].qty;
        }
        else
        {
            metrics_.best_ask_px = 0;
            metrics_.best_ask_qty = 0;  
        }
    });

    return add_result_t{ .status=status, .order_id=order_id, .trades=std::move(trades), .filled_qty=filled_qty, .remaining_qty=remaining_qty};
}

template<class Sync>
bool cancel_impl(Sync& sync, OrderBook& ob_, engine_metrics_t& metrics_, id_t order_id)
{
    bool is_ok = false;
    sync.write([&]{
        is_ok = ob_.cancel(order_id);
        if(is_ok)
        {
            metrics_.cancel_orders++;
        }
    });
    return is_ok; 
}

// ------------Engine Implementation---------
// V1: simple single thread implementation
// stop for further derivation. For safe capsulation, make it final.
class EngineSingleThreaded final: public IEngine {
public:
    explicit EngineSingleThreaded(const engine_config_t& config): config_(config) {}
    add_result_t add_order(const order_cmd_t& cmd) override
    {
        single_thread_sync_t sync;
        return add_impl(sync, config_, ob_, metrics_, next_, seq_, cmd);
    };
    bool cancel_order(id_t order_id) override 
    { 
        single_thread_sync_t sync;
        return cancel_impl(sync, ob_, metrics_, order_id);
    };
    snapshot_t snapshot(int depth) const override {return ob_.snapshot(depth);};

    engine_metrics_t metrics() const override { return metrics_; }

private:
    engine_config_t config_;
    OrderBook ob_;
    id_t next_{1000};
    uint64_t seq_{0}; // internal sequence number for ordering
    mutable engine_metrics_t metrics_;
};

// --------------- Asynchronous Engine Implementation (Producer/Consumer) -----------------
// V2: SPSC implementation
class EngineAsync final : public IEngine {
public:
    explicit EngineAsync(const engine_config_t& config, std::size_t queue_capacity = (1u << 16)) 
        : config_(config), queue_(queue_capacity)
    {
        start();
    }

private:
    enum class kind_t : uint8_t {ADD, CANCEL, STOP};  ///< ADD: add new order, CANCEL: cancel order, STOP: quit loop
    
    /**
     * @brief command format within asynchronous engine with spsc ring
     * 
     */
    struct cmd_t {
        kind_t kind{}; ///< type of command
        // ADD
        order_cmd_t add_cmd{}; ///< add order command
        std::promise<add_result_t>* add_reply{nullptr}; ///< asynchronous result from add command. Consumer informs producer
        // CANCEL
        id_t cancel_id{}; ///< cancel order command
        std::promise<bool>* cancel_reply{nullptr}; ///< asynchronous result from cancel command. Consumer informs producer
    };

    engine_config_t config_;

    // state of mutex
    mutable std::shared_mutex state_mtx_;
    OrderBook ob_;
    id_t next_{1000};
    uint64_t seq_{0};
    mutable engine_metrics_t metrics_{}; // measurement

    // circular queue to store order_cmt_t type data
    concurrency::SpscRing<cmd_t> queue_;
    std::atomic<bool> running_{false};
    std::thread worker_;

    void start()
    {
        bool expected = false;
        // make sure only one thread is created
        if (!running_.compare_exchange_strong(expected, true)) {return;}
        worker_ = std::thread(&EngineAsync::run_loop_consumer, this);
    }

    void stop()
    {
        bool expected = true;
        // single thread can only be closed once
        if(!running_.compare_exchange_strong(expected, false)) {return;}

        // send one time stop to force thread quit
        cmd_t cmd;
        cmd.kind = kind_t::STOP;
        queue_.push(std::move(cmd));

        if(worker_.joinable()) {worker_.join();}
    }

    void run_loop_consumer();

    add_result_t add_order(const order_cmd_t& cmd) override
    {
        shared_mutex_sync_t sync{state_mtx_};
        return add_impl(sync, config_, ob_, metrics_, next_, seq_, cmd);
    }

    bool cancel_order(const id_t order_id) override
    {
        shared_mutex_sync_t sync{state_mtx_};
        return cancel_impl(sync, ob_, metrics_, order_id);
    }

    snapshot_t snapshot(int depth) const override
    {
        shared_mutex_sync_t sync{state_mtx_};
        return sync.read([&]{return ob_.snapshot(depth);});
    }

    engine_metrics_t metrics() const override {
        shared_mutex_sync_t sync{state_mtx_};
        return sync.read([&]{return metrics_;});
    }
};

/**
 * @brief main loop of consumer
 * 
 */
void EngineAsync::run_loop_consumer()
{
    while(running_.load(std::memory_order_acquire))
    {
        cmd_t cmd;
        // no tasks to do -> yield until next operation
        if(!queue_.pop(cmd))
        {
            std::this_thread::yield();
            continue;
        }

        switch(cmd.kind)
        {
            case kind_t::ADD: {
                add_result_t result = add_order(cmd.add_cmd);
                if(cmd.add_reply != nullptr) {cmd.add_reply->set_value(std::move(result));} // send back asynchronously result
                break;
            }
            case kind_t::CANCEL: {
                bool is_ok = cancel_order(cmd.cancel_id);
                if(cmd.cancel_reply != nullptr) {cmd.cancel_reply->set_value(is_ok);} // send back asynchronously result
                break;
            }
            case kind_t::STOP:
            default:
            {
                break;
            }
        }
    }

    // final step after receiving STOP command
    cmd_t cmd;
    while (queue_.pop(cmd))
    {
        // clean up rest unhandled command
        if(cmd.kind == kind_t::ADD && cmd.add_reply != nullptr)
        {
            add_result_t result;
            result.status = OrderStatus::REJECT;
            cmd.add_reply->set_value(std::move(result));
        }
        if(cmd.kind == kind_t::CANCEL && cmd.cancel_reply != nullptr)
        {
            cmd.cancel_reply->set_value(std::move(false));
        }
    }
}

#ifndef SCOPEX_ENGINE_IMPL_ASYNC
#define SCOPEX_ENGINE_IMPL_ASYNC 1
#endif

/// create a unique pointer of engine
/// for future extension, can create different engine implementations based on config
std::unique_ptr<IEngine> make_engine(const engine_config_t& config)
{
#if SCOPEX_ENGINE_IMPL_ASYNC
    return std::make_unique<EngineAsync>(config);
#else
    return std::make_unique<EngineSingleThreaded>(config);
#endif
}

} // namespace engine