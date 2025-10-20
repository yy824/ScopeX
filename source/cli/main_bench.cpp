#include <libs/engine/engine.hpp>
#include <fmt/format.h>
#include <atomic>
#include <cstdint>
#include <random>
#include <vector>
#include <algorithm>
#include <chrono>

using namespace engine;

namespace cli_bench{
struct args 
{
    std::uint32_t n_orders = 200000;    /**< Number of orders to generate */
    std::uint8_t seed = 42; /**< Seed for random number generation */
    std::uint8_t hot_levels = 5; /**< hot levels of price (±N ticks around mid price) */
    std::uint16_t max_qty = 100; /**< maximum quantity per order */
    price_t mid_price = 10000; /**< mid price in ticks */
    std::uint8_t depth = 5; /**< Depth of the order book snapshot */
    bool warmup = true; /**< warmup */
};
}; //namespace cli_bench

using namespace cli_bench;

int main(int argc, char** argv)
{
    args args_value{};

    // parse arguments
    for(int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "--n-orders" && ( i + 1 < argc ))
        {
            args_value.n_orders = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--seed" && ( i + 1 < argc ))
        {
            args_value.seed = static_cast<std::uint8_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--hot-levels" && ( i + 1 < argc ))
        {
            args_value.hot_levels = static_cast<std::uint8_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--max-qty" && ( i + 1 < argc ))
        {
            args_value.max_qty = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--mid-price" && ( i + 1 < argc ))
        {
            args_value.mid_price = static_cast<price_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--depth" && ( i + 1 < argc ))
        {
            args_value.depth = static_cast<std::uint8_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--no-warmup")
        {
            args_value.warmup = false;
        }
    }

    auto eng = make_engine(engine_config_t{true, 0});

    // random generator
    std::mt19937 rng(args_value.seed);
    std::uniform_int_distribution<int> side_dist(0, 1); // 0: BUY, 1: SELL
    std::uniform_int_distribution<int> time_in_force_dist(0, 2); // 0: GTC, 1: IOC, 2: FOK
    std::uniform_int_distribution<int> price_level_dist(-args_value.hot_levels, args_value.hot_levels); // price levels around mid price
    std::uniform_int_distribution<int> qty_dist(1, args_value.max_qty); // quantity distribution

    // ----- create order flow (simple model) -----
    std::vector<order_cmd_t> flow;
    flow.reserve(args_value.n_orders);

    for(std::uint32_t i = 0; i < args_value.n_orders; i++)
    {
        order_cmd_t order_cmd{};
        order_cmd.side = (side_dist(rng) == 0) ? Side::BUY : Side::SELL;
        order_cmd.order_type = OrderType::LIMIT;
        order_cmd.time_in_force = (time_in_force_dist(rng) == 0) ? TimeInForce::GTC : 
                                   (time_in_force_dist(rng) == 1) ? TimeInForce::IOC : TimeInForce::FOK;
        order_cmd.price = args_value.mid_price + static_cast<price_t>(price_level_dist(rng));
        order_cmd.qty = static_cast<qty_t>(qty_dist(rng));

        flow.emplace_back(order_cmd);
    }

    // ----- warmup -----
    if(args_value.warmup)
    {
        for(int i = 0; i < 5000; i++)
        {
            eng->add_order(flow[i]);
        }
    }

    // ----- stress test main -----
    std::vector<uint64_t> latencies_ns;
    latencies_ns.reserve(args_value.n_orders);

    const auto t_start = std::chrono::high_resolution_clock::now();
    for(const auto& order_cmd : flow)
    {
        const auto t_oc_start = std::chrono::high_resolution_clock::now();
        eng->add_order(order_cmd);
        const auto t_oc_end = std::chrono::high_resolution_clock::now();
        const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_oc_end - t_oc_start);
        latencies_ns.push_back(duration_ns.count());
    }

    const auto t_end = std::chrono::high_resolution_clock::now();
    
    // get result
    const auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    // throughtput million per second
    const auto throughput_mops = static_cast<double>(args_value.n_orders) / (static_cast<double>(total_duration_ms) / 1000.0);

    // latency statistics
    auto get_percentile = [&](double percent) -> uint64_t {
        std::vector<uint64_t> temp = latencies_ns;
        auto idx = static_cast<uint8_t>((percent / 100.0) * static_cast<double>(temp.size() - 1));
        std::nth_element(temp.begin(), temp.begin() + idx, temp.end());
        return temp[idx];
    };

    auto metric = eng->metrics();
    auto snap = eng->snapshot(args_value.depth);

    fmt::print("=== BENCH TEST ===\n");
    fmt::print("orders={} total_ms = {} throughput_mops={:.3f}\n", 
        args_value.n_orders, total_duration_ms, throughput_mops);
    fmt::print("latency_ns: p50={} p90={} p99={} min={} max={}\n", 
        get_percentile(50.0), get_percentile(90.0), get_percentile(99.0), metric.add_min_ns, metric.add_max_ns);
    fmt::print("best_bid: price={} qty={}\n", metric.best_bid_px, metric.best_bid_qty);
    fmt::print("best_ask: price={} qty={}\n", metric.best_ask_px, metric.best_ask_qty);

    fmt::print("SNAPSHOT depth={}\nBIDS:\n", args_value.depth);
    for(const auto& bid : snap.bids)
    {
        fmt::print("price={} qty={}\n", bid.price, bid.qty);
    }
    fmt::print("ASKS:\n");
    for(const auto& ask : snap.asks)
    {
        fmt::print("price={} qty={}\n", ask.price, ask.qty);
    }
    return 0;
}

