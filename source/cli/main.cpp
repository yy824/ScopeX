#include <fmt/core.h>
#include <libs/engine/engine.hpp>
#include <fmt/format.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include <cctype>

using namespace engine;

namespace {
    /**
     * @brief Enum class representing the columns in the CSV file for order book replay.
     * 
     */
    enum class csv_columns: uint8_t {
        TIMESTAMP = 0,   /**< Timestamp of the order */
        CMD,             /**< Command type (e.g., NEW, CANCEL) */
        SIDE,            /**< Side of the order (e.g., BUY, SELL) */
        ORDER_TYPE,      /**< Type of the order (e.g., LIMIT, MARKET) */
        TIME_IN_FORCE,   /**< Time in force for the order (e.g., GTC, IOC) */
        PRICE,           /**< Price of the order */
        QTY,             /**< Quantity of the order */
        ORDER_ID,        /**< Unique identifier for the order */
        // total columns count
        COUNT            /**< Total number of columns */
    }; 

    /**
     * @brief Structure representing the command line arguments.
     * 
     */
    struct args {
        std::string replay_file;    /**< Path to the replay file */
        std::string out_file;       /**< Path to the output file */
        int depth = 5;              /**< Depth of the order book */
        bool print_trades = false;  /**< Flag to print trades */
        bool print_metrics = true;  /**< Flag to print metrics */
        bool no_human = false;      /**< Flag to disable human-readable output */
    }; 

    /**
     * @brief Case-insensitive string comparison.
     * 
     * @param str_a First string to compare.
     * @param str_b Second string to compare.
     * @return true If the strings are equal (case-insensitive).
     * @return false If the strings are not equal.
     */
    auto inline ieq(const std::string& str_a, const std::string& str_b) -> bool
    {
        if(str_a.size() != str_b.size()) { return false; }
        for(size_t i = 0; i < str_a.size(); i++)
        {
            if(std::tolower(static_cast<unsigned char>(str_a[i])) != std::tolower(static_cast<unsigned char>(str_b[i])))
            { return false; }
        }
        return true;
    };

    // remove head and tail spaces
    auto trim(std::string str) -> std::string
    {
        auto isspace2 = [](unsigned char chara){return std::isspace(chara);};
        while(!str.empty() && (isspace2(str.front()) != 0)) {str.erase(str.begin());}
        while(!str.empty() && (isspace2(str.back()) != 0)) {str.pop_back();}
        return str;
    }

    auto split_csv_line(const std::string& line) -> std::vector<std::string>
    {
        // for simple CSV without quoted fields
        std::vector<std::string> result;
        std::stringstream s_stream(line);
        std::string cell;
        while(std::getline(s_stream, cell, ',')) {
            // remove head/tail spaces for each cell
            result.push_back(trim(cell));
        }
        return result; 
    }

    auto parse_args(int argc, char** argv) -> std::optional<args>
    {
        args result;
        for(int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg == "--replay" && ( i + 1 < argc ))
            {
                result.replay_file = argv[++i]; // jump to the next argument
            }
            else if (arg == "--depth" && ( i + 1 < argc ))
            {
                result.depth = std::stoi(argv[++i]);
            }
            else if (arg == "--print-trades")
            {
                result.print_trades = true;
            }
            else if (ieq(arg, "--no-metrics"))
            {
                result.print_metrics = false;
            }
            else if (ieq(arg, "--no-human"))
            {
                result.no_human = true;
            }
            else if (arg == "--out" && ( i + 1 < argc ))
            {
                result.out_file = argv[++i]; // jump to the next argument
            }
            else if(arg == "-h" || arg == "--help")
            {
                fmt::print("Usage: scopex_cli --replay <replay_file> [--depth <n>] [--print-trades] [--no-metrics]\n", argv[0]);
                return std::nullopt;
            }
        }

        if(result.replay_file.empty())
        {
            fmt::print(stderr, "Error: --replay <replay_file> is required\n");
            return std::nullopt;
        }

        return result;
    }

    struct metrics_t {
        uint64_t orders_add = 0;
        uint64_t orders_cancel = 0;
        uint64_t trades = 0;
        uint64_t traded_qty = 0;
    };
}; // anonymous namespace

int main(int argc, char** argv)
{
    auto parsed_args = parse_args(argc, argv);
    if(!parsed_args.has_value()) { return 2; }
    
    args args_value = *parsed_args;

    std::ifstream infile(args_value.replay_file);
    if(!infile.is_open())
    {
        fmt::print(stderr, "Error: cannot open replay file {}\n", args_value.replay_file);
        return 2;
    }

    // create engine instance
    auto engine = make_engine({/*market_gtc_as_ioc*/.market_gtc_as_ioc=true, /*markets_max_levels*/.market_max_levels=0});

    metrics_t metric{};
    std::string line;
    bool first_line = true;

    while(std::getline(infile, line))
    {
        // do cleanup for the line in order to get # or spaces only
        line = trim(line);
        if(line.empty() || line[0] == '#') {continue;} // skip empty lines or comments
        auto cells = split_csv_line(line);

        // parsing header line or skip it
        if(first_line) 
        {
            if(!cells.empty() && ieq(cells[0], "timestamp"))
            {
                // get first line
                first_line = false; 
                continue;
            }
            // error code 
            fmt::print(stderr, "Error: invalid first line (not header): {}\n", line); 
            return 3;
        }

        if(cells.size() < 3)
        {
            fmt::print(stderr, "Warning: invalid line (too few columns): {}\n", line);
            continue;
        }

        // cmd column
        const std::string& cmd = cells[1];

        if(ieq(cmd, "ADD"))
        {
            // timestamp,cmd,side,order_type,time_in_force,price,qty[,order_id], at least 7 columns
            if(cells.size() < static_cast<size_t>(csv_columns::ORDER_ID))
            {
                fmt::print(stderr, "Warning: invalid ADD line (too few columns): {}\n", line);
                continue;
            }

            const std::string& side_str = cells[static_cast<size_t>(csv_columns::SIDE)];
            const std::string& order_type_str = cells[static_cast<size_t>(csv_columns::ORDER_TYPE)];
            const std::string& tif_str = cells[static_cast<size_t>(csv_columns::TIME_IN_FORCE)];
            const std::string& price_str = cells[static_cast<size_t>(csv_columns::PRICE)];
            const std::string& qty_str = cells[static_cast<size_t>(csv_columns::QTY)];
            const std::string& order_id_str = (cells.size() >= static_cast<size_t>(csv_columns::ORDER_ID) + 1 ? cells[static_cast<size_t>(csv_columns::ORDER_ID)] : "");

            order_cmd_t order_cmd{};
            order_cmd.side = ieq(side_str, "BUY") ? Side::BUY : (Side::SELL);
            order_cmd.order_type =  ieq(order_type_str, "LIMIT") ? OrderType::LIMIT : OrderType::MARKET;
            order_cmd.time_in_force = ieq(tif_str, "IOC") ? TimeInForce::IOC : (ieq(tif_str, "FOK") ? TimeInForce::FOK : TimeInForce::GTC);

            order_cmd.price = price_str.empty() ? 0 : static_cast<price_t>(std::stoll(price_str));
            order_cmd.qty = static_cast<qty_t>(std::stoll(qty_str));

            // optional order_id field - if empty, engine will assign with offset automatically
            if(!order_id_str.empty())
            {
                order_cmd.order_id = static_cast<engine::id_t>(std::stoull(order_id_str));
            }

            // status, order_id, trades, filled_qty, remaining_qty 
            auto order_result = engine->add_order(order_cmd);
            metric.orders_add++;

            fmt::print("===============================\n");
            fmt::print("ADD order: timestamp={} side={} order_type={} time_in_force={} price={} qty={}\n", 
                cells[0], side_str, order_type_str, tif_str, order_cmd.price, order_cmd.qty);
            fmt::print("-------------------------------\n");
            fmt::print("order_id={} status={}\n", order_result.order_id, std::to_string(static_cast<int>(order_result.status)));
            // parsing all handled trades for metrics which is independent from status (bad status = no trades)
            for(auto& trade: order_result.trades)
            {
                metric.trades++;
                metric.traded_qty += trade.qty;
                if (args_value.print_trades) 
                {
                    fmt::print("TRADE taker={} maker={} price={:.2f} quantity={} timestamp={}\n", 
                        trade.taker, trade.maker, static_cast<double>(trade.price)/100.0, trade.qty, trade.timestamp);
                }
            }

        } else if (ieq(cmd, "CANCEL"))
        {
            // timestamp,cmd,order_id
            const std::string& order_id_str = cells.back();

            // for cancel, order_id is required. Order_id is in the last column and able to be parsed to uint64_t
            if(order_id_str.empty())
            {
                fmt::print(stderr, "Warning: invalid CANCEL line (missing order_id): {}\n", line);
                continue;
            }

            bool is_ok = engine->cancel_order(static_cast<engine::id_t>(std::stoull(order_id_str)));
            if(is_ok) { metric.orders_cancel++; }
            else
            {
                fmt::print(stderr, "Warning: CANCEL failed (not found): {}\n", line);
            }

        } else
        {
            fmt::print(stderr, "Warning: unknown command (not ADD or CANCEL): {}\n", line);
            continue;
        }
    }

    // print final snapshot
    auto snap = engine->snapshot(args_value.depth);
    
    fmt::print("===== Order Book snapshot_t (top {} levels) =====\n", args_value.depth);
    fmt::print("BIDs: \n");
    for(auto& level : snap.bids)
    {
        fmt::print("  price={:.2f} qty={}\n", static_cast<double>(level.price)/100.0, level.qty);
    }
    fmt::print("ASKs: \n");
    for(auto& level : snap.asks)
    {
        fmt::print("  price={:.2f} qty={}\n", static_cast<double>(level.price)/100.0, level.qty);
    }
    fmt::print("=====================================\n");

    if(args_value.print_metrics)
    {
        fmt::print("===== Metrics =====\n");
        fmt::print("Orders added: {}\n", metric.orders_add);
        fmt::print("Orders canceled: {}\n", metric.orders_cancel);
        fmt::print("Trades executed: {}\n", metric.trades);
        fmt::print("Total traded quantity: {}\n", metric.traded_qty);
        fmt::print("===================\n");
    }

    return 0;
}