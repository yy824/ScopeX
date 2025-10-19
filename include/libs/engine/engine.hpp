/*
 * @Author: lyan liuyanbc157_cn@hotmail.com
 * @Date: 2025-09-26 11:49:58
 * @LastEditors: lyan liuyanbc157_cn@hotmail.com
 * @LastEditTime: 2025-09-30 16:59:21
 * @FilePath: \scopeX1\source\libs\engine\engine.hpp
 * @Description: 
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>
#include <memory>
#include <map>
#include <deque>

namespace engine {


// --------- Value Types ---------
using price_t = int64_t; // ticks price -> e.g., price=12345 means 123.45 if tick size is 0.01
using qty_t = int64_t;  // quantity
using id_t = uint64_t; // unique identifier

enum class Side : uint8_t { BUY, SELL };
enum class OrderType : uint8_t { LIMIT, MARKET };
enum class TimeInForce : uint8_t { GTC, IOC, FOK }; // Good-Til-Canceled, Immediate-Or-Cancel, Fill-Or-Kill
enum class OrderStatus : uint8_t { OK, PARTIAL, FILLED, REJECT, FOK_FAIL, EMPTY_BOOK, BAD_INPUT };

// --------- Data Structures ---------

/**
 * @brief engine::OrderCmd is a structure representing a command to create new orders, with fields for optional order ID, side (defaulting to BUY), order type (defaulting to LIMIT), time-in-force (defaulting to GTC), price (for LIMIT orders), quantity, and an optional user-provided timestamp. This structure is used to encapsulate all necessary details for defining and submitting an order.
 * 
 */
struct order_cmd_t {
    std::optional<id_t> order_id = std::nullopt; ///< optional order id
    Side side{Side::BUY}; ///< side of the order (default: BUY)
    OrderType order_type{OrderType::LIMIT}; ///< type of the order (default: LIMIT)
    TimeInForce time_in_force{TimeInForce::GTC}; ///< time in force (default: GTC)
    price_t price{0}; ///< price for LIMIT orders
    qty_t qty{0}; ///< quantity
    uint64_t timestamp{0}; ///< optional user timestamp
};

/**
 * @brief engine::order_t is a structure representing an order in a trading system, containing fields for a unique order ID, side (BUY or SELL), price, quantity, and an internal sequence number for ordering. This structure encapsulates all necessary details of an order for processing within the order book.
 * 
 */
struct order_t {
    id_t id{}; ///< unique order identifier
    Side side{}; ///< side of the order (BUY or SELL)
    price_t price{}; ///< price of the order
    qty_t qty{}; ///< quantity of the order
    uint64_t seq_num{}; ///< internal sequence number for ordering - now timestamp is used
};

/**
 * @brief engine::trade_t is a structure representing a trade execution in a trading system, containing fields for the taker order ID, maker order ID, trade price, trade quantity, and a timestamp indicating when the trade occurred. This structure encapsulates all necessary details of a trade for record-keeping and processing.
 * 
 */
struct trade_t {
    id_t taker{}; ///< order id of the taker: from bid list
    id_t maker{}; ///< order id of the maker: from ask list
    price_t price{}; ///< price of the trade
    qty_t qty{}; ///< quantity of the trade
    uint64_t timestamp{}; ///< trade execution time
};

/**
 * @brief engine::snapshot_level_t represents a single level in an order book snapshot, containing fields for the price and quantity at that level. This structure is used to encapsulate the details of each price level in the order book for both bids and asks.
 * 
 */
struct snapshot_level_t {
    price_t price{}; ///< price level of snapshot
    qty_t qty{}; ///< quantity at this price level
};

/**
 * @brief engine::snapshot_t represents a snapshot of the order book, containing vectors of bid and ask levels. Each level is represented by a snapshot_level_t structure, which includes the price and quantity at that level. The bids vector is sorted in descending order by price, while the asks vector is sorted in ascending order by price. This structure provides a comprehensive view of the current state of the order book.
 * 
 */
struct snapshot_t {
    std::vector<snapshot_level_t> bids; ///< sorted descending by price
    std::vector<snapshot_level_t> asks; ///< sorted ascending by price
};

/**
 * @brief engine::add_result_t represents the result of adding an order to the order book, containing the status of the operation and the ID of the newly created order (if successful).
 * 
 */
struct add_result_t {
    OrderStatus status{OrderStatus::OK}; ///< status of the add order operation
    id_t order_id{}; ///< id of the newly created order (if successful)
    std::vector<trade_t> trades; ///< trades executed as a result of this order
    qty_t filled_qty{0}; ///< quantity filled immediately
    qty_t remaining_qty{0}; ///< quantity remaining in the book
};

/**
 * @brief engine::locate_t is a structure used to locate an order within the order book, containing fields for the side of the order (BUY or SELL), iterators pointing to the price level in the bid or ask book, and an iterator pointing to the specific order within the price level's queue. This structure facilitates efficient access and management of orders in the order book.
 * 
 */
struct locate_t {
    Side side; ///< side of the order (BUY or SELL)
    std::map<price_t, std::deque<order_t>, std::greater<>>::iterator bid_it; ///< point to price node (iterator) in order book map
    std::map<price_t, std::deque<order_t>, std::less<>>::iterator ask_it; ///< point to price node (iterator) in order book map
    std::deque<order_t>::iterator q_it;     ///< point to order in the price level queue
};

/**
 * @brief engine::engine_metrics_t is a structure for tracking various performance and state metrics of the trading engine, including counts of added and canceled orders, trade statistics, order book state hints, and latency statistics for order additions.
 * 
 */
struct engine_metrics_t {
    // volume & counts
    std::uint64_t add_orders = 0; ///< number of orders added
    std::uint64_t cancel_orders = 0; ///< number of orders canceled
    std::uint64_t trades = 0; ///< number of trades executed
    std::uint64_t traded_qty = 0; ///< total quantity traded

    // order_book state hints
    std::uint64_t best_bid_px = 0; ///< best bid price
    std::uint64_t best_bid_qty = 0; ///< best bid quantity
    std::uint64_t best_ask_px = 0; ///< best ask price
    std::uint64_t best_ask_qty = 0; ///< best ask quantity

    // latency rough stats (ns)
    // basic statistics: min/max/avg
    std::uint64_t add_min_ns = std::numeric_limits<std::uint64_t>::max(); ///< mininum nanoseconds
    std::uint64_t add_max_ns = 0; ///< maximum nanoseconds
    std::uint64_t add_total_ns = 0; ///< total nanoseconds
};

// --------- Engine Interface ---------

/// @brief Engine configuration options
struct engine_config_t {
    bool market_gtc_as_ioc{true}; ///< MARKET + GTC : true -> IOC by default, false -> REJECT
    uint64_t market_max_levels{0}; ///< optional: Max levels in market depth snapshot
};

class IEngine {
    public:
    virtual ~IEngine() = default;
    virtual add_result_t add_order(const order_cmd_t& cmd) = 0;
    virtual bool cancel_order(id_t order_id) = 0;
    virtual snapshot_t snapshot(int depth) const = 0;
    virtual engine_metrics_t metrics() const = 0;
};

// Factory function to create an engine instance
std::unique_ptr<IEngine> make_engine(const engine_config_t& config = {});

}  // namespace engine
