#include "../src/core/order_book.hpp"
#include <x86intrin.h>
#include <vector>
#include <iostream>
#include <algorithm>

inline uint64_t rdtsc() {
    return __rdtsc();
}

void bench_best_price() {
    OrderBook ob(90, 110, 100000);

    // preload some orders
    for (int i = 0; i < 1000; ++i) {
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i, 100 + (i % 5), 10, OrderSide::Bid});
    }

    std::vector<uint64_t> samples;
    samples.reserve(1'000'000);

    for (int i = 0; i < 1'000'000; ++i) {
        uint64_t t0 = rdtsc();

        PriceLevel pl;
        ob.getBestBid(pl);

        uint64_t t1 = rdtsc();
        samples.push_back(t1 - t0);
    }

    std::sort(samples.begin(), samples.end());

    std::cout << "BestBid P50: " << samples[samples.size()*0.50] << " cycles\n";
    std::cout << "BestBid P95: " << samples[samples.size()*0.95] << " cycles\n";
    std::cout << "BestBid P99: " << samples[samples.size()*0.99] << " cycles\n";
}

void bench_insert() {
    OrderBook ob(90, 110, 2'000'000);

    std::vector<uint64_t> samples;
    samples.reserve(1'000'000);

    for (int i = 0; i < 1'000'000; ++i) {
        MarketUpdate u{0, UpdateType::Add, (uint64_t)i, 100 + (i % 10), 10, OrderSide::Bid};

        uint64_t t0 = rdtsc();
        ob.applyUpdate(u);
        uint64_t t1 = rdtsc();

        samples.push_back(t1 - t0);
    }

    std::sort(samples.begin(), samples.end());

    std::cout << "Insert P50: " << samples[samples.size()*0.50] << " cycles\n";
    std::cout << "Insert P95: " << samples[samples.size()*0.95] << " cycles\n";
    std::cout << "Insert P99: " << samples[samples.size()*0.99] << " cycles\n";
}

void bench_modify_qty() {
    OrderBook ob(90, 110, 2'000'000);

    // preload orders
    for (int i = 0; i < 1'000'000; ++i) {
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i, 100 + (i % 10), 10, OrderSide::Bid});
    }

    std::vector<uint64_t> samples;
    samples.reserve(1'000'000);

    for (int i = 0; i < 1'000'000; ++i) {
        MarketUpdate u{0, UpdateType::Modify, (uint64_t)i, 100 + (i % 10), 7, OrderSide::Bid};

        uint64_t t0 = rdtsc();
        ob.applyUpdate(u);
        uint64_t t1 = rdtsc();

        samples.push_back(t1 - t0);
    }

    std::sort(samples.begin(), samples.end());

    std::cout << "ModifyQty P50: " << samples[samples.size()*0.50] << " cycles\n";
    std::cout << "ModifyQty P95: " << samples[samples.size()*0.95] << " cycles\n";
    std::cout << "ModifyQty P99: " << samples[samples.size()*0.99] << " cycles\n";
}

void bench_modify_price() {
    OrderBook ob(90, 110, 2'000'000);

    // preload orders
    for (int i = 0; i < 1'000'000; ++i) {
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i, 100, 10, OrderSide::Bid});
    }

    std::vector<uint64_t> samples;
    samples.reserve(1'000'000);

    for (int i = 0; i < 1'000'000; ++i) {
        MarketUpdate u{0, UpdateType::Modify, (uint64_t)i, 101, 10, OrderSide::Bid};

        uint64_t t0 = rdtsc();
        ob.applyUpdate(u);
        uint64_t t1 = rdtsc();

        samples.push_back(t1 - t0);
    }

    std::sort(samples.begin(), samples.end());

    std::cout << "ModifyPrice P50: " << samples[samples.size()*0.50] << " cycles\n";
    std::cout << "ModifyPrice P95: " << samples[samples.size()*0.95] << " cycles\n";
    std::cout << "ModifyPrice P99: " << samples[samples.size()*0.99] << " cycles\n";
}

void bench_cancel() {
    OrderBook ob(90, 110, 2'000'000);

    // preload orders
    for (int i = 0; i < 1'000'000; ++i) {
        ob.applyUpdate({0, UpdateType::Add, (uint64_t)i, 100 + (i % 10), 10, OrderSide::Bid});
    }

    std::vector<uint64_t> samples;
    samples.reserve(1'000'000);

    for (int i = 0; i < 1'000'000; ++i) {
        MarketUpdate u{0, UpdateType::Cancel, (uint64_t)i, 0, 0, OrderSide::Bid};

        uint64_t t0 = rdtsc();
        ob.applyUpdate(u);
        uint64_t t1 = rdtsc();

        samples.push_back(t1 - t0);
    }

    std::sort(samples.begin(), samples.end());

    std::cout << "Cancel P50: " << samples[samples.size()*0.50] << " cycles\n";
    std::cout << "Cancel P95: " << samples[samples.size()*0.95] << " cycles\n";
    std::cout << "Cancel P99: " << samples[samples.size()*0.99] << " cycles\n";
}

int main() {
    bench_best_price();
    bench_insert();
    bench_modify_qty();
    bench_modify_price();
    bench_cancel();
    return 0;
}

