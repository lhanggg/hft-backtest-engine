#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>

// 32‑byte feed record
#pragma pack(push, 1)
struct FeedRecord {
    uint64_t ts;
    uint32_t  type;
    uint32_t  side;
    uint64_t order_id;
    int64_t  price;
    int64_t  qty;
    uint32_t _pad = 0; // padding to reach 48 bytes
    uint32_t _pad2 = 0;
};
#pragma pack(pop)

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
        FeedRecord rec{};
        rec.ts = std::chrono::steady_clock::now().time_since_epoch().count();
        rec.type = static_cast<uint8_t>(type_dist(rng));
        rec.side = static_cast<uint8_t>(side_dist(rng));
        rec.order_id = i + 1;
        rec.price = 10000 + price_dist(rng);
        rec.qty = qty_dist(rng);

        out.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
    }

    std::cout << "Generated " << num << " messages into " << filename << "\n";
    return 0;
}