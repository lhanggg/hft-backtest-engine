#include <iostream>
#include <cstdlib>

#include "core/order_book.hpp"
#include "core/ring_buffer.hpp"
#include "engine/event_loop.hpp"
#include "engine/imbalance_strategy.hpp"
#include "feed/feed_handler.hpp"
#include "replay/mmap_replay.hpp"
#include "risk/risk_manager.hpp"
#include "util/timer.hpp"

// ---------------------------------------------------------------------------
// run_backtest — drives the full pipeline with ImbalanceStrategy
//
// Usage:
//   run_backtest <feed_file> [ema_alpha] [threshold]
//
//   ema_alpha  : EMA smoothing factor    (default 0.1,  range 0–1)
//   threshold  : imbalance trigger level (default 0.3,  range 0–1)
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: run_backtest <feed_file> [ema_alpha] [threshold]\n";
        return 1;
    }
    const char* filename  = argv[1];
    double      ema_alpha = (argc >= 3) ? std::atof(argv[2]) : 0.1;
    double      threshold = (argc >= 4) ? std::atof(argv[3]) : 0.3;

    std::cout << "=== Backtest: ImbalanceStrategy ===\n";
    std::cout << "Feed     : " << filename  << "\n";
    std::cout << "EMA α    : " << ema_alpha << "\n";
    std::cout << "Threshold: " << threshold << "\n\n";

    constexpr std::size_t QUEUE_CAP = 1u << 20;

    SpscRing<MarketUpdate>   md_queue(QUEUE_CAP);
    SpscRing<StrategySignal> out_queue(QUEUE_CAP);

    // Price range must match generate_feed: price = 10000 ± 50
    OrderBook           ob(9900, 10100, 2'000'000);
    RiskManager         risk(/*max_price*/ 20000, /*max_qty*/ 10);
    ImbalanceStrategy   strategy(ob, ema_alpha, threshold);
    FeedHandler         fh(md_queue);
    EventLoop           loop(md_queue, out_queue, ob, strategy, risk,
                             /*timer_interval_ns*/ UINT64_MAX);  // disable periodic timer

    // Load feed into SPSC queue, then run engine
    std::uint64_t num_msgs = run_mmap_replay(fh, filename);

    const std::uint64_t t0 = get_monotonic_ns();
    loop.run();
    const std::uint64_t t1 = get_monotonic_ns();

    double elapsed = (t1 - t0) / 1e9;

    std::cout << "=== Results ===\n";
    std::cout << "Messages  : " << num_msgs             << "\n";
    std::cout << "Updates   : " << loop.updates_processed() << "\n";
    std::cout << "Elapsed   : " << elapsed              << " s\n";
    if (elapsed > 0.0)
        std::cout << "Throughput: " << loop.updates_processed() / elapsed
                  << " updates/sec\n";
    std::cout << "\n";
    strategy.print_summary();

    return 0;
}
