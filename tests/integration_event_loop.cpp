#include "util/cpu_affinity.hpp"
#include "core/ring_buffer.hpp"
#include "core/order_book.hpp"
#include "engine/event_loop.hpp"
#include "risk/risk_manager.hpp"
#include "engine/strategy_interface.hpp"
#include "engine/strategy_example.cpp"
#include "replay/mmap_replay.hpp"
#include "feed/feed_handler.hpp"
#include "util/timer.hpp"

#include <thread>
#include <iostream>

int main() {
    std::cout << "integration_event_loop main starting\n";

    pin_thread_to_core(2);
    std::cout << "after pin_thread_to_core\n";

    SpscRing<MarketUpdate>   md_queue(1 << 20);
    SpscRing<StrategySignal> out_queue(1 << 20);
    OrderBook order_book(9900, 10100, 2'000'000);
    RiskManager risk(1'000'000, 10);
    DummyStrategy strategy(out_queue, 10);
    EventLoop loop(md_queue, out_queue, order_book, strategy, risk,
                   /*timer_interval_ns*/ 100'000);
    FeedHandler feed_handler(md_queue);

    run_mmap_replay(feed_handler, "sample_feed.bin");

    const std::uint64_t start_ns = get_monotonic_ns();
    loop.run();
    const std::uint64_t end_ns = get_monotonic_ns();

    const double seconds = (end_ns - start_ns) / 1e9;
    const std::uint64_t updates = loop.updates_processed();

    std::cout << "=== EventLoop Stats ===\n";
    std::cout << "Updates processed: " << updates << "\n";
    std::cout << "Elapsed time: " << seconds << " sec\n";
    if (seconds > 0.0) {
        std::cout << "Throughput: "
                  << (updates / seconds)
                  << " updates/sec\n";
    } else {
        std::cout << "Throughput: n/a (elapsed time = 0)\n";
    }

    std::cout << "main returning\n";
    return 0;
}