#pragma once

#include <cstdint>

struct MarketUpdate;

struct StrategySignal {
    std::int64_t price = 0;
    std::int64_t qty   = 0;
};

class Strategy {
public:
    virtual ~Strategy() = default;

    // Called after the order book has been updated with this MarketUpdate.
    virtual void on_market_update(const MarketUpdate& mu) = 0;

    // Optional periodic callback from the event loop (e.g., every N microseconds).
    virtual void on_timer(std::uint64_t timestamp_ns) { (void)timestamp_ns; }

    // Called by the engine to fetch a pending signal (if any).
    // Returns true if a signal was produced.
    virtual bool poll_signal(StrategySignal& out) { (void)out; return false; }
};