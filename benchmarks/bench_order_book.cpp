#include "../src/core/order_book.hpp"
#include <x86intrin.h>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <chrono>

// ---------------------------------------------------------------------------
// TSC calibration
// Spin for ~100 ms and compare TSC ticks against wall clock.
// Returns nanoseconds per TSC cycle.
// ---------------------------------------------------------------------------
static double calibrate_ns_per_cycle() {
    using clk = std::chrono::steady_clock;

    volatile uint64_t dummy = __rdtsc(); (void)dummy;  // serialize

    auto     w0 = clk::now();
    uint64_t t0 = __rdtsc();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               clk::now() - w0).count() < 100) {}
    uint64_t t1 = __rdtsc();
    auto     w1 = clk::now();

    double wall_ns = (double)std::chrono::duration_cast<
                         std::chrono::nanoseconds>(w1 - w0).count();
    return wall_ns / (double)(t1 - t0);
}

// ---------------------------------------------------------------------------
// Print sorted percentiles in nanoseconds
// ---------------------------------------------------------------------------
static void print_stats(const char* name,
                        std::vector<uint64_t>& s,
                        double ns_per_cyc)
{
    std::sort(s.begin(), s.end());
    size_t n = s.size();
    printf("%-18s  p50=%6.1f ns  p99=%7.1f ns  p99.9=%7.1f ns  max=%8.1f ns\n",
           name,
           s[n * 50  / 100 ] * ns_per_cyc,
           s[n * 99  / 100 ] * ns_per_cyc,
           s[n * 999 / 1000] * ns_per_cyc,
           (double)s.back()  * ns_per_cyc);
}

static constexpr size_t N      = 1'000'000;
static constexpr size_t WARMUP =    50'000;

// ---------------------------------------------------------------------------
void bench_best_price(double ns) {
    OrderBook ob(90, 110, 10'000);
    for (int i = 0; i < 1'000; ++i)
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i, 100 + (i % 5), 10, OrderSide::Bid});

    PriceLevel pl;
    for (size_t i = 0; i < WARMUP; ++i) ob.getBestBid(pl);   // i-cache warmup

    std::vector<uint64_t> samples(N);
    for (size_t i = 0; i < N; ++i) {
        uint64_t t0 = __rdtsc();
        ob.getBestBid(pl);
        uint64_t t1 = __rdtsc();
        samples[i] = t1 - t0;
    }
    print_stats("getBestBid", samples, ns);
}

// ---------------------------------------------------------------------------
void bench_insert(double ns) {
    OrderBook ob(90, 110, WARMUP + N + 10);

    // warmup: prime i-cache and branch predictor
    for (size_t i = 0; i < WARMUP; ++i)
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i, 100 + (i % 10), 10, OrderSide::Bid});

    std::vector<uint64_t> samples(N);
    for (size_t i = 0; i < N; ++i) {
        MarketUpdate u{0, UpdateType::Add, (uint64_t)(WARMUP + i),
                       100 + (int64_t)(i % 10), 10, OrderSide::Bid};
        uint64_t t0 = __rdtsc();
        ob.applyUpdate(u);
        uint64_t t1 = __rdtsc();
        samples[i] = t1 - t0;
    }
    print_stats("insert", samples, ns);
}

// ---------------------------------------------------------------------------
void bench_modify_qty(double ns) {
    OrderBook ob(90, 110, N + 10);
    for (size_t i = 0; i < N; ++i)
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i, 100 + (int64_t)(i % 10), 10, OrderSide::Bid});

    // warmup: same-price modify (fast path)
    for (size_t i = 0; i < WARMUP; ++i)
        ob.applyUpdate({0, UpdateType::Modify, (uint64_t)(i % N),
                        100 + (int64_t)(i % 10), 8, OrderSide::Bid});

    std::vector<uint64_t> samples(N);
    for (size_t i = 0; i < N; ++i) {
        MarketUpdate u{0, UpdateType::Modify, (uint64_t)i,
                       100 + (int64_t)(i % 10), 6, OrderSide::Bid};
        uint64_t t0 = __rdtsc();
        ob.applyUpdate(u);
        uint64_t t1 = __rdtsc();
        samples[i] = t1 - t0;
    }
    print_stats("modify-qty", samples, ns);
}

// ---------------------------------------------------------------------------
void bench_cancel(double ns) {
    // Insert 2*N orders; warmup by cancelling the second half, then
    // measure cancelling the first half.
    OrderBook ob(90, 110, 2 * N + WARMUP + 10);

    for (size_t i = 0; i < 2 * N; ++i)
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i,
                        100 + (int64_t)(i % 10), 10, OrderSide::Bid});

    // warmup: cancel orders [N .. N+WARMUP)
    for (size_t i = N; i < N + WARMUP; ++i)
        ob.applyUpdate({0, UpdateType::Cancel, (uint64_t)i, 0, 0, OrderSide::Bid});

    // measure: cancel orders [0 .. N)
    std::vector<uint64_t> samples(N);
    for (size_t i = 0; i < N; ++i) {
        MarketUpdate u{0, UpdateType::Cancel, (uint64_t)i, 0, 0, OrderSide::Bid};
        uint64_t t0 = __rdtsc();
        ob.applyUpdate(u);
        uint64_t t1 = __rdtsc();
        samples[i] = t1 - t0;
    }
    print_stats("cancel", samples, ns);
}

// ---------------------------------------------------------------------------
int main() {
    printf("Calibrating TSC... ");
    fflush(stdout);
    double ns = calibrate_ns_per_cycle();
    printf("%.3f ns/cycle  (%.2f GHz)\n\n", ns, 1.0 / ns);

    printf("%-18s  %-20s  %-21s  %-21s  %s\n",
           "benchmark", "p50", "p99", "p99.9", "max");
    printf("%-18s  %-20s  %-21s  %-21s  %s\n",
           "---------", "---", "---", "-----", "---");

    bench_best_price(ns);
    bench_insert(ns);
    bench_modify_qty(ns);
    bench_cancel(ns);

    return 0;
}
