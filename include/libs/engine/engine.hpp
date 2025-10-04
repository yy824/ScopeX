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
using price_t = std::int64_t; // ticks price -> e.g., price=12345 means 123.45 if tick size is 0.01
using qty_t = std::int64_t;  // quantity
using id_t = std::uint64_t; // unique identifier

enum class Side : std::uint8_t { BUY, SELL };
enum class OrderType : std::uint8_t { LIMIT, MARKET };
enum class TimeInForce : std::uint8_t { GTC, IOC, FOK }; // Good-Til-Canceled, Immediate-Or-Cancel, Fill-Or-Kill
enum class OrderStatus : std::uint8_t { OK, PARTIAL, FILLED, REJECT, FOK_FAIL, EMPTY_BOOK, BAD_INPUT };

// --------- Data Structures ---------

/* engine::OrderCmd is a structure representing a command to create new orders, with fields for optional order ID, side (defaulting to BUY), order type (defaulting to LIMIT), time-in-force (defaulting to GTC), price (for LIMIT orders), quantity, and an optional user-provided timestamp. This structure is used to encapsulate all necessary details for defining and submitting an order. */
struct order_cmd_t {
    std::optional<id_t> order_id = std::nullopt; /* if not set, means new order */
    Side side{Side::BUY};
    OrderType order_type{OrderType::LIMIT};
    TimeInForce time_in_force{TimeInForce::GTC};
    price_t price{0}; // for LIMIT orders
    qty_t qty{0};
    std::uint64_t timestamp{0}; // optional user timestamp
};

struct order_t {
    id_t id{};
    Side side{};
    price_t price{};
    qty_t qty{};
    std::uint64_t seq_num{}; // internal sequence number for ordering
};

struct trade_t {
    id_t taker{};
    id_t maker{}; 
    price_t price{};
    qty_t qty{};
    std::uint64_t timestamp{}; // trade execution time
};

struct snapshot_level_t {
    price_t price{};
    qty_t qty{};
};

struct snapshot_t {
    std::vector<snapshot_level_t> bids; // sorted descending by price
    std::vector<snapshot_level_t> asks; // sorted ascending by price
};

struct add_result_t {
    OrderStatus status{OrderStatus::OK};
    id_t order_id{};
    std::vector<trade_t> trades; // trades executed as a result of this order
    qty_t filled_qty{0}; // quantity filled immediately
    qty_t remaining_qty{0}; // quantity remaining in the book
};

struct locate_t {
    Side side;
    std::map<price_t, std::deque<order_t>, std::greater<>>::iterator bid_it;    // point to price node (iterator) in order book map
    std::map<price_t, std::deque<order_t>, std::less<>>::iterator ask_it;
    std::deque<order_t>::iterator q_it;     // point to order in the price level queue
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
};

// Factory function to create an engine instance
std::unique_ptr<IEngine> make_engine(const engine_config_t& config = {});

}  // namespace engine
