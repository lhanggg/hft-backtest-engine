#include "engine/event_loop.hpp"

#include "core/order_book.hpp"
#include "core/ring_buffer.hpp"
#include "util/timer.hpp"
#include <iostream>

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
    last_timer_ts_ns_ = get_monotonic_ns();
}

void EventLoop::run() {
    while (true) {
        bool did_work = false;

        did_work |= handle_market_data();
        did_work |= handle_strategy_output();

        const std::uint64_t now_ns = get_monotonic_ns();
        maybe_fire_timer(now_ns);

        if (!did_work) {
            break;
        }
    }
}

bool EventLoop::handle_market_data() {
    bool did_work = false;

    MarketUpdate mu;
    while (md_queue_.pop(mu)) {
        did_work = true;
        ++updates_processed_;
        order_book_.applyUpdate(mu);
        strategy_.on_market_update(mu);
    }

    return did_work;
}

bool EventLoop::handle_strategy_output() {
    bool did_work = false;

    StrategySignal sig;
    while (strategy_.poll_signal(sig)) {
        did_work = true;
        if (risk_.check(sig)) {
            out_queue_.push(sig);
        }
    }

    return did_work;
}

void EventLoop::maybe_fire_timer(std::uint64_t now_ns) {
    if (now_ns - last_timer_ts_ns_ >= timer_interval_ns_) {
        strategy_.on_timer(now_ns);
        last_timer_ts_ns_ = now_ns;
    }
}