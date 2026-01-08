#pragma once

#include <cstdint>

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
};