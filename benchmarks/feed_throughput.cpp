#include <iostream>
#include <chrono>

#include "../src/core/order_book.hpp"
#include "../src/core/ring_buffer.hpp"
#include "../src/feed/feed_handler.hpp"

// Forward-declare run_mmap_replay from mmap_replay.cpp
std::uint64_t run_mmap_replay(FeedHandler& fh, const char* filename);

// Minimal Week-5 consumer loop: drain queue and apply to order book.
static void drain_queue_until_empty(MdQueue& queue, OrderBook& ob) {
    MarketUpdate u;
    while (queue.pop(u)) {
        ob.applyUpdate(u);
    }
}

int main(int argc, char** argv) {
    std::cout << "STARTING BENCHMARK\n";
    if (argc < 2) {
        std::cerr << "Usage: feed_throughput <replay_file>\n";
        return 1;
    }
    const char* filename = argv[1];

    // Tune as you like; power-of-two is good for SPSC.
    constexpr std::size_t QUEUE_CAP = 1u << 20;

    MdQueue queue(QUEUE_CAP);

    // Use realistic parameters consistent with your microbench.
    OrderBook ob(/*min_price*/90, /*max_price*/110, /*max_orders*/2'000'000);

    FeedHandler fh(queue);

    auto t0 = std::chrono::high_resolution_clock::now();

    // Producer: mmap replay → FeedHandler → SPSC queue
    std::uint64_t num_msgs = run_mmap_replay(fh, filename);

    // Consumer (Week 5 style): drain everything into the order book
    drain_queue_until_empty(queue, ob);

    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = t1 - t0;

    double seconds = elapsed.count();
    if (seconds > 0.0 && num_msgs > 0) {
        double mps = num_msgs / seconds;
        std::cout << "Replayed " << num_msgs << " messages in "
                  << seconds << " seconds\n";
        std::cout << "Throughput: " << mps << " messages/sec\n";
    } else {
        std::cout << "No messages processed or zero elapsed time.\n";
    }

    return 0;
}