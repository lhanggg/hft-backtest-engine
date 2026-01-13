#pragma once

#include <cstdint>
#include <type_traits>


enum class UpdateType : uint8_t {
    Add,
    Modify,
    Cancel
};

enum class OrderSide : uint8_t {
    Bid,
    Ask
};

struct MarketUpdate {
    uint64_t ts;        // feed or replay timestamp
    UpdateType type;
    uint64_t order_id;
    int64_t  price;
    int64_t  qty;
    OrderSide side;
    uint8_t  reserved[6]; // padding to make struct 32 bytes
};

static_assert(std::is_trivially_copyable<MarketUpdate>::value, "MarketUpdate must be trivially copyable");
static_assert(sizeof(MarketUpdate) == 40 || sizeof(MarketUpdate) == 48, "Expect compact fixed size (40 or 48 bytes)");
