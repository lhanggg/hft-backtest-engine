#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <limits>

#include "../src/core/order_book.hpp"
#include "../src/core/ring_buffer.hpp"
#include "../src/feed/feed_handler.hpp"
#include "../src/util/cpu_affinity.hpp"

// Forward-declare run_mmap_replay from mmap_replay.cpp
std::uint64_t run_mmap_replay(FeedHandler& fh, const char* filename);

// Sentinel: no affinity requested for this thread.
static constexpr std::uint32_t NO_AFFINITY = std::numeric_limits<std::uint32_t>::max();

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: feed_throughput <replay_file> [producer_core consumer_core]\n";
        std::cerr << "  Omit core args to run without thread affinity (OS decides).\n";
        return 1;
    }
    const char*    filename       = argv[1];
    const bool     pin_threads    = (argc >= 4);
    std::uint32_t  producer_core  = pin_threads ? (std::uint32_t)std::atoi(argv[2]) : NO_AFFINITY;
    std::uint32_t  consumer_core  = pin_threads ? (std::uint32_t)std::atoi(argv[3]) : NO_AFFINITY;

    constexpr std::size_t QUEUE_CAP = 1u << 20;   // power-of-two for SPSC mask trick

    MdQueue   queue(QUEUE_CAP);
    OrderBook ob(/*min_price*/90, /*max_price*/110, /*max_orders*/2'000'000);
    FeedHandler fh(queue);

    // Shared state between threads — written by one side, read after join.
    std::uint64_t          num_produced = 0;
    std::uint64_t          num_consumed = 0;
    std::atomic<bool>      producer_done{false};

    auto t0 = std::chrono::high_resolution_clock::now();

    // -----------------------------------------------------------------------
    // Producer thread: mmap replay → FeedHandler → SPSC queue
    // -----------------------------------------------------------------------
    std::thread producer_thread([&] {
        if (producer_core != NO_AFFINITY) pin_thread_to_core(producer_core);
        num_produced = run_mmap_replay(fh, filename);
        producer_done.store(true, std::memory_order_release);
    });

    // -----------------------------------------------------------------------
    // Consumer thread: SPSC queue → OrderBook::applyUpdate
    // Spin until producer signals done AND queue is drained.
    // -----------------------------------------------------------------------
    std::thread consumer_thread([&] {
        if (consumer_core != NO_AFFINITY) pin_thread_to_core(consumer_core);
        MarketUpdate u;
        while (true) {
            if (queue.pop(u)) {
                ob.applyUpdate(u);
                ++num_consumed;
            } else if (producer_done.load(std::memory_order_acquire)) {
                // Producer is finished — drain any items remaining in the queue.
                while (queue.pop(u)) {
                    ob.applyUpdate(u);
                    ++num_consumed;
                }
                break;
            }
            // else: queue transiently empty, producer still running — spin
        }
    });

    producer_thread.join();
    consumer_thread.join();   // happens-before: safe to read num_produced/consumed

    auto t1 = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "Affinity      : " << (pin_threads ? "pinned" : "none (OS schedules)") << "\n";
    if (pin_threads) {
        std::cout << "Producer core : " << producer_core << "\n";
        std::cout << "Consumer core : " << consumer_core << "\n";
    }
    std::cout << "Produced      : " << num_produced  << " msgs\n";
    std::cout << "Consumed      : " << num_consumed  << " msgs\n";

    if (seconds > 0.0 && num_produced > 0) {
        double mps = (double)num_produced / seconds;
        std::cout << "Elapsed       : " << seconds        << " s\n";
        std::cout << "Throughput    : " << mps / 1e6      << " M msgs/sec\n";
    } else {
        std::cout << "No messages processed or zero elapsed time.\n";
    }

    return 0;
}
