#include "util/cpu_affinity.hpp"
#include "core/ring_buffer.hpp"
#include "core/order_book.hpp"
#include "engine/event_loop.hpp"
#include "risk/risk_manager.hpp"
#include "engine/strategy_interface.hpp"
#include "engine/strategy_example.cpp"
#include "replay/mmap_replay.hpp"
#include "feed/feed_handler.hpp"

#include <thread>

int main() {
    // 1) Pin this thread (event loop owner) to a core.
    pin_thread_to_core(2);  // core 2 is an example; we’ll discuss choosing it below.

    // 2) Allocate hot data structures AFTER pinning.
    SpscRing<MarketUpdate>   md_queue(1 << 20);
    SpscRing<StrategySignal> out_queue(1 << 20);
    OrderBook order_book(90, 110, 2'000'000); // realistic params
    RiskManager risk(1'000'000, 10);

    DummyStrategy strategy(out_queue, 10);

    // 3) Construct event loop.
    EventLoop loop(md_queue, out_queue, order_book, strategy, risk,
                   /*timer_interval_ns*/ 100'000);

    // 4) Start replay in another thread pinned to a nearby core.
    FeedHandler feed_handler(md_queue);
    std::thread replay_thread([&] {
        pin_thread_to_core(3);       // close to 2, same NUMA node ideally
        run_mmap_replay(feed_handler, "sample_feed.bin"); // your existing function
    });

    // 5) Run event loop on this core.
    loop.run();

    replay_thread.join();
    return 0;
}