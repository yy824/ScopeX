/*
 * @Author: lyan liuyanbc157_cn@hotmail.com
 * @Date: 2025-09-26 11:49:58
 * @LastEditors: lyan liuyanbc157_cn@hotmail.com
 * @LastEditTime: 2025-09-30 16:59:21
 * @FilePath: \scopeX1\source\libs\engine\engine.hpp
 * @Description: 
 */
#pragma once

#include <optional>
#include <vector>
#include <memory>
#include <map>
#include <deque>

namespace engine {


// --------- Value Types ---------
using Price = std::int64_t; // ticks price -> e.g., price=12345 means 123.45 if tick size is 0.01
using Qty = std::int64_t;  // quantity
using Id = std::uint64_t; // unique identifier

enum class Side { BUY, SELL };
enum class OrderType { LIMIT, MARKET };
enum class TimeInForce { GTC, IOC, FOK }; // Good-Til-Canceled, Immediate-Or-Cancel, Fill-Or-Kill
enum class OrderStatus { OK, PARTIAL, FILLED, REJECT, FOK_FAIL, EMPTY_BOOK, BAD_INPUT };

// --------- Data Structures ---------

/* engine::OrderCmd is a structure representing a command to create new orders, with fields for optional order ID, side (defaulting to BUY), order type (defaulting to LIMIT), time-in-force (defaulting to GTC), price (for LIMIT orders), quantity, and an optional user-provided timestamp. This structure is used to encapsulate all necessary details for defining and submitting an order. */
struct OrderCmd {
    std::optional<Id> orderId; /* if not set, means new order */
    Side side{Side::BUY};
    OrderType orderType{OrderType::LIMIT};
    TimeInForce timeInForce{TimeInForce::GTC};
    Price price{0}; // for LIMIT orders
    Qty qty{0};
    std::uint64_t timestamp{0}; // optional user timestamp
};

struct Order {
    Id id{};
    Side side{};
    Price price{};
    Qty qty{};
    std::uint64_t seq_num{}; // internal sequence number for ordering
};

struct Trade {
    Id taker{};
    Id maker{}; 
    Price price{};
    Qty qty{};
    std::uint64_t timestamp{}; // trade execution time
};

struct SnapshotLevel {
    Price price{};
    Qty qty{};
};

struct Snapshot {
    std::vector<SnapshotLevel> bids; // sorted descending by price
    std::vector<SnapshotLevel> asks; // sorted ascending by price
};

struct AddResult {
    OrderStatus status{OrderStatus::OK};
    Id orderId{};
    std::vector<Trade> trades; // trades executed as a result of this order
    Qty filledQty{0}; // quantity filled immediately
    Qty remainingQty{0}; // quantity remaining in the book
};

struct Locate {
    Side side;
    std::map<Price, std::deque<Order>, std::greater<>>::iterator bid_it;    // point to price node (iterator) in order book map
    std::map<Price, std::deque<Order>, std::less<>>::iterator ask_it;
    std::qqdafdf<Order>::iterator q_it;     // point to order in the price level queue
};


// --------- Engine Interface ---------

/// @brief Engine configuration options
struct EngineConfig {
    bool market_gtc_as_ioc{true}; ///< MARKET + GTC : true -> IOC by default, false -> REJECT
    size_t market_max_levels{0}; ///< optional: Max levels in market depth snapshot
};

class IEngine {
    public:
    virtual ~IEngine() = default;
    virtual AddResult addOrder(const OrderCmd& cmd) = 0;
    virtual bool cancelOrder(Id orderId) = 0;
    virtual Snapshot snapshot(int depth = 5)  const = 0;  
};

// Factory function to create an engine instance
std::unique_ptr<IEngine> make_engine(const EngineConfig& config = {});

}  // namespace engine
