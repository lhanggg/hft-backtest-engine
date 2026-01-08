#include "engine/event_loop.hpp"

#include "core/order_book.hpp"
#include "util/timer.hpp"         
#include "core/ring_buffer.hpp" 
#include <iostream>

// ---- EventLoop implementation ----

EventLoop::EventLoop(MdQueue&      md_queue,
                     OutQueue&     out_queue,
                     OrderBook&    order_book,
                     Strategy&     strategy,
                     RiskManager&  risk_manager,
                     std::uint64_t timer_interval_ns)
    : md_queue_(md_queue)
    , out_queue_(out_queue)
    , order_book_(order_book)
    , strategy_(strategy)
    , risk_(risk_manager)
    , timer_interval_ns_(timer_interval_ns)
{
    last_timer_ts_ns_ = get_monotonic_ns(); // from util/timer.hpp
}

void EventLoop::run() {
    while (running_.load(std::memory_order_acquire)) {
        const std::uint64_t now_ns = get_monotonic_ns();
        handle_market_data();
        handle_strategy_output();
        maybe_fire_timer(now_ns);

        // If you want a pure busy-poll loop, leave this empty.
        // Otherwise, you can add a tiny pause or backoff here for tests.
    }
}

void EventLoop::handle_market_data() {
    MarketUpdate mu;
    // Drain a batch of updates per iteration for throughput.
    while (md_queue_.pop(mu)) {
        std::cout << "EventLoop got update: " << mu.price << "\n";
        order_book_.applyUpdate(mu);
        strategy_.on_market_update(mu);
    }
}

void EventLoop::handle_strategy_output() {
    StrategySignal sig;
    // Strategy might have at most one pending signal at a time for Week 7.
    while (strategy_.poll_signal(sig)) {
        std::cout << "EventLoop got strategy signal: " << sig.price << "\n";
        if (risk_.check(sig)) {
            // In a real engine this would be an outbound order queue.
            // For the backtester, you can store these for later verification.
            out_queue_.push(sig);
        }
    }
}

void EventLoop::maybe_fire_timer(std::uint64_t now_ns) {
    if (now_ns - last_timer_ts_ns_ >= timer_interval_ns_) {
        strategy_.on_timer(now_ns);
        last_timer_ts_ns_ = now_ns;
    }
}