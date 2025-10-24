#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <csignal>
#include "libs/engine/engine.hpp"

using namespace std::chrono_literals;
using engine::Side;
using engine::OrderType;
using engine::TimeInForce;
using engine::order_cmd_t;

static bool g_running = true;

void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);

    engine::engine_config_t cfg;
    cfg.market_gtc_as_ioc = true;
    cfg.market_max_levels = 10;

    auto eng = engine::make_engine(cfg); // 工厂函数返回 EngineAsync 或 EngineSingleThreaded

    std::cout << "Starting M2 demo (Ctrl+C to stop)\n";

    // --- Producer Thread ---
    std::thread producer([&] {
        std::mt19937_64 rng{std::random_device{}()};
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<int> price_dist(99, 101);
        std::uniform_int_distribution<int> qty_dist(1, 5);

        uint64_t counter = 0;
        while (g_running) {
            order_cmd_t cmd;
            cmd.order_type = OrderType::LIMIT;
            cmd.side = (side_dist(rng) == 0 ? Side::BUY : Side::SELL);
            cmd.price = price_dist(rng);
            cmd.qty = qty_dist(rng);
            cmd.time_in_force = TimeInForce::GTC;

            eng->add_order(cmd);
            counter++;

            if (counter % 200 == 0)
                std::this_thread::sleep_for(5ms);
        }
    });

    // --- Monitor Thread ---
    std::thread monitor([&] {
        while (g_running) {
            auto snap = eng->snapshot(3);
            auto metrics = eng->metrics();

            std::cout << "\n--- Snapshot ---\n";
            std::cout << "Bids:\n";
            for (auto& b : snap.bids)
                std::cout << "  " << b.price << " x" << b.qty << "\n";
            std::cout << "Asks:\n";
            for (auto& a : snap.asks)
                std::cout << "  " << a.price << " x" << a.qty << "\n";

            std::cout << "--- Metrics ---\n";
            std::cout << "Orders: " << metrics.add_orders
                      << " Trades: " << metrics.trades
                      << " TradedQty: " << metrics.traded_qty << "\n";
            std::cout.flush();

            std::this_thread::sleep_for(500ms);
        }
    });

    // --- Wait for exit ---
    while (g_running)
        std::this_thread::sleep_for(200ms);

    std::cout << "\nStopping...\n";
    producer.join();
    monitor.join();

    std::cout << "All threads stopped.\n";
    return 0;
}