#pragma once

#include <cstdint>
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
    using MdQueue  = SpscRing<MarketUpdate>;
    using OutQueue = SpscRing<StrategySignal>;

    EventLoop(MdQueue&      md_queue,
              OutQueue&     out_queue,
              OrderBook&    order_book,
              Strategy&     strategy,
              RiskManager&  risk_manager,
              std::uint64_t timer_interval_ns);

    // Drain all available work until queues are empty.
    void run();

    std::uint64_t updates_processed() const noexcept {
        return updates_processed_;
    }

private:
    bool handle_market_data();
    bool handle_strategy_output();
    void maybe_fire_timer(std::uint64_t now_ns);

private:
    MdQueue&     md_queue_;
    OutQueue&    out_queue_;
    OrderBook&   order_book_;
    Strategy&    strategy_;
    RiskManager& risk_;

    std::uint64_t last_timer_ts_ns_{0};
    std::uint64_t timer_interval_ns_{0};

    std::uint64_t updates_processed_ = 0;
};