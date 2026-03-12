#pragma once

#include "market_data.hpp"
#include <cstdint>
#include <cstddef>
#include <limits>
#include <vector>

struct alignas(64) OrderNode {
  uint64_t order_id;   // unique identifier
  int64_t  price;      // price in ticks
  int32_t  qty;        // remaining quantity
  uint32_t next;       // index of next node in list, or INVALID_INDEX
  uint32_t prev;       // index of prev node in list, or INVALID_INDEX
  OrderSide side;      // bid or ask
  uint8_t  _pad[35];   // padding to keep struct 64 bytes and aligned

  static constexpr uint32_t INVALID_INDEX =
      std::numeric_limits<uint32_t>::max();
};

struct alignas(64) PriceLevel {
  uint32_t head;  // index of first order in this level
  uint32_t tail;  // index of last order (optional but helpful)
  int64_t  price; // price for this level (redundant but convenient)
  int64_t  total_qty; // aggregated quantity at this level

  PriceLevel()
    : head(OrderNode::INVALID_INDEX),
      tail(OrderNode::INVALID_INDEX),
      price(0),
      total_qty(0) {}
};

class OrderBook {
public:
    OrderBook(int64_t min_price,
              int64_t max_price,
              size_t   max_orders);

    ~OrderBook();

    OrderBook(const OrderBook&)            = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&)                 = delete;
    OrderBook& operator=(OrderBook&&)      = delete;

    // main entry point
    void applyUpdate(const MarketUpdate& u);

    // queries
    [[nodiscard]] bool getBestBid(PriceLevel& out) const;
    [[nodiscard]] bool getBestAsk(PriceLevel& out) const;

private:
    // configuration
    int64_t min_price_;
    int64_t max_price_;
    size_t  num_levels_;
    size_t  max_orders_;
    // price levels
    PriceLevel* bids_;
    PriceLevel* asks_;

    // order nodes
    OrderNode* nodes_;

    // free list for nodes
    uint32_t free_head_;

    std::vector<uint32_t> id_to_index_;
    // best prices
    int64_t best_bid_price_;
    int64_t best_ask_price_;

    // helpers (we will implement later)
    uint32_t allocNode();
    void     freeNode(uint32_t idx);

    void insertOrder(const MarketUpdate& u);
    void modifyOrder(const MarketUpdate& u);
    void cancelOrder(const MarketUpdate& u);
};
