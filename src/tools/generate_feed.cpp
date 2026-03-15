#include "core/market_data.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: generate_feed <output_file> <num_messages>\n";
        return 1;
    }

    const char* filename = argv[1];
    uint64_t num = std::strtoull(argv[2], nullptr, 10);

    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file\n";
        return 1;
    }

    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<int> type_dist(0, 2);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> price_dist(-50, 50);
    std::uniform_int_distribution<int> qty_dist(1, 100);

    for (uint64_t i = 0; i < num; ++i) {
        MarketUpdate mu{};
        mu.ts       = static_cast<uint64_t>(
                          std::chrono::steady_clock::now().time_since_epoch().count());
        mu.type     = static_cast<UpdateType>(type_dist(rng));
        mu.side     = static_cast<OrderSide>(side_dist(rng));
        mu.order_id = i + 1;
        mu.price    = 10000 + price_dist(rng);
        mu.qty      = qty_dist(rng);

        out.write(reinterpret_cast<const char*>(&mu), sizeof(mu));
    }

    std::cout << "Generated " << num << " messages into " << filename << "\n";
    return 0;
}