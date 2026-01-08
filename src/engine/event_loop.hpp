#pragma once

#include <cstdint>
#include <atomic>

#include "engine/strategy_interface.hpp"
#include "risk/risk_manager.hpp"

// Forward declarations.
struct MarketUpdate;
class OrderBook;

// You can swap this out for your actual ring buffer / lock-free queue type.
template <typename T>
class SpscRing;

class EventLoop {
public:
    using MdQueue   = SpscRing<MarketUpdate>;
    using OutQueue  = SpscRing<StrategySignal>;

    EventLoop(MdQueue&       md_queue,
              OutQueue&      out_queue,
              OrderBook&     order_book,
              Strategy&      strategy,
              RiskManager&   risk_manager,
              std::uint64_t  timer_interval_ns);

    // Runs until `stop()` is called, or until input queue is drained
    // if you implement a special run_until_empty() variant.
    void run();

    void stop() { running_.store(false, std::memory_order_release); }

private:
    void handle_market_data();
    void handle_strategy_output();
    void maybe_fire_timer(std::uint64_t now_ns);

private:
    MdQueue&      md_queue_;
    OutQueue&     out_queue_;
    OrderBook&    order_book_;
    Strategy&     strategy_;
    RiskManager&  risk_;

    std::atomic<bool> running_{true};

    std::uint64_t last_timer_ts_ns_{0};
    std::uint64_t timer_interval_ns_{0};
};