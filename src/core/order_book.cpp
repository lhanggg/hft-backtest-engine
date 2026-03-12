#include "order_book.hpp"

#include <iostream>

OrderBook::OrderBook(int64_t min_price,
                     int64_t max_price,
                     size_t  max_orders)
    : min_price_(min_price),
      max_price_(max_price),
      max_orders_(max_orders),
      best_bid_price_(min_price),
      best_ask_price_(max_price)
{
    id_to_index_.resize(max_orders_, OrderNode::INVALID_INDEX);

    num_levels_ = static_cast<size_t>(max_price_ - min_price_ + 1);

    // allocate price levels
    bids_ = new PriceLevel[num_levels_];
    asks_ = new PriceLevel[num_levels_];

    // allocate nodes
    nodes_ = new OrderNode[max_orders_];
    // initialize free list
    free_head_ = 0;
    for (size_t i = 0; i < max_orders_; ++i) {
        nodes_[i].next = (i + 1 < max_orders_) ? i + 1 : OrderNode::INVALID_INDEX;
    }
}

OrderBook::~OrderBook() {
    delete[] bids_;
    delete[] asks_;
    delete[] nodes_;
}

uint32_t OrderBook::allocNode() {
    if (free_head_ == OrderNode::INVALID_INDEX) {
        // out of memory 
        return OrderNode::INVALID_INDEX;
    }

    uint32_t idx = free_head_;
    free_head_ = nodes_[idx].next;   // move head to next free node
    return idx;
}

void OrderBook::freeNode(uint32_t idx) {
    nodes_[idx].next = free_head_;   // push this node onto the free list
    free_head_ = idx;
}

void OrderBook::insertOrder(const MarketUpdate& u) {
    // 1. allocate node
    uint32_t idx = allocNode();
    if (idx == OrderNode::INVALID_INDEX) return;

    // 2. fill node fields
    OrderNode& node = nodes_[idx];
    node.order_id = u.order_id;
    node.price    = u.price;
    node.qty      = u.qty;
    node.side     = u.side;
    node.next     = OrderNode::INVALID_INDEX;
    node.prev     = OrderNode::INVALID_INDEX;

    // 3. map price → level index
    size_t level_idx = static_cast<size_t>(u.price - min_price_);

    PriceLevel* levels = (u.side == OrderSide::Bid) ? bids_ : asks_;
    PriceLevel& level  = levels[level_idx];

    // 4. insert into price level (FIFO)
    if (level.head == OrderNode::INVALID_INDEX) {
        // empty level
        level.head = idx;
        level.tail = idx;
        level.price = u.price;
    } else {
        // append to tail
        node.prev = level.tail;
        nodes_[level.tail].next = idx;
        level.tail = idx;
    }

    // 5. update total quantity
    level.total_qty += u.qty;

    // 6. update best bid/ask
    if (u.side == OrderSide::Bid) {
        if (u.price > best_bid_price_) {
            best_bid_price_ = u.price;
        }
    } else {
        if (u.price < best_ask_price_) {
            best_ask_price_ = u.price;
        }
    }

    // 7. store id → index mapping
    id_to_index_[u.order_id] = idx;

}

void OrderBook::modifyOrder(const MarketUpdate& u) {
    // 1. find node
    uint32_t idx = id_to_index_[u.order_id];
    if (idx == OrderNode::INVALID_INDEX) return;

    OrderNode& node = nodes_[idx];
    // 2. quantity-only modify
    if (u.price == node.price) {
        int32_t delta = u.qty - node.qty;
        node.qty = u.qty;

        size_t level_idx = static_cast<size_t>(node.price - min_price_);
        PriceLevel* levels = (node.side == OrderSide::Bid) ? bids_ : asks_;
        PriceLevel& level = levels[level_idx];

        level.total_qty += delta;
        return;
    }

    // 3. price change: remove from old level (O(1) via prev/next)
    size_t old_idx = static_cast<size_t>(node.price - min_price_);
    PriceLevel* old_levels = (node.side == OrderSide::Bid) ? bids_ : asks_;
    PriceLevel& old_level = old_levels[old_idx];

    {
        uint32_t p = node.prev;
        uint32_t n = node.next;
        if (p == OrderNode::INVALID_INDEX) old_level.head = n;
        else                               nodes_[p].next = n;
        if (n == OrderNode::INVALID_INDEX) old_level.tail = p;
        else                               nodes_[n].prev = p;
    }

    old_level.total_qty -= node.qty;

    // 4. update best price if needed
    if (node.side == OrderSide::Bid && node.price == best_bid_price_) {
        // scan downward to find next non-empty bid
        for (int64_t p = best_bid_price_; p >= min_price_; --p) {
            size_t li = static_cast<size_t>(p - min_price_);
            if (bids_[li].head != OrderNode::INVALID_INDEX) {
                best_bid_price_ = p;
                break;
            }
        }
    }

    if (node.side == OrderSide::Ask && node.price == best_ask_price_) {
        // scan upward to find next non-empty ask
        for (int64_t p = best_ask_price_; p <= max_price_; ++p) {
            size_t li = static_cast<size_t>(p - min_price_);
            if (asks_[li].head != OrderNode::INVALID_INDEX) {
                best_ask_price_ = p;
                break;
            }
        }
    }

    // 5. update node fields
    node.price = u.price;
    node.qty   = u.qty;
    node.next  = OrderNode::INVALID_INDEX;
    node.prev  = OrderNode::INVALID_INDEX;

    // 6. insert into new price level
    size_t new_idx = static_cast<size_t>(u.price - min_price_);
    PriceLevel* new_levels = (node.side == OrderSide::Bid) ? bids_ : asks_;
    PriceLevel& new_level = new_levels[new_idx];

    if (new_level.head == OrderNode::INVALID_INDEX) {
        new_level.head = idx;
        new_level.tail = idx;
    } else {
        node.prev = new_level.tail;
        nodes_[new_level.tail].next = idx;
        new_level.tail = idx;
    }

    new_level.total_qty += node.qty;

    // 7. update best price for new level
    if (node.side == OrderSide::Bid && u.price > best_bid_price_) {
        best_bid_price_ = u.price;
    }
    if (node.side == OrderSide::Ask && u.price < best_ask_price_) {
        best_ask_price_ = u.price;
    }
}

void OrderBook::cancelOrder(const MarketUpdate& u) {
    // 1. find node
    uint32_t idx = id_to_index_[u.order_id];
    if (idx == OrderNode::INVALID_INDEX) return;

    OrderNode& node = nodes_[idx];
    // 2. find price level
    size_t level_idx = static_cast<size_t>(node.price - min_price_);
    PriceLevel* levels = (node.side == OrderSide::Bid) ? bids_ : asks_;
    PriceLevel& level = levels[level_idx];

    // 3. unlink node from list (O(1) via prev/next)
    {
        uint32_t p = node.prev;
        uint32_t n = node.next;
        if (p == OrderNode::INVALID_INDEX) level.head = n;
        else                               nodes_[p].next = n;
        if (n == OrderNode::INVALID_INDEX) level.tail = p;
        else                               nodes_[n].prev = p;
    }

    // 4. update total quantity
    level.total_qty -= node.qty;

    // 5. update best price if needed
    if (node.side == OrderSide::Bid && node.price == best_bid_price_) {
        for (int64_t p = best_bid_price_; p >= min_price_; --p) {
            size_t li = static_cast<size_t>(p - min_price_);
            if (bids_[li].head != OrderNode::INVALID_INDEX) {
                best_bid_price_ = p;
                break;
            }
        }
    }

    if (node.side == OrderSide::Ask && node.price == best_ask_price_) {
        for (int64_t p = best_ask_price_; p <= max_price_; ++p) {
            size_t li = static_cast<size_t>(p - min_price_);
            if (asks_[li].head != OrderNode::INVALID_INDEX) {
                best_ask_price_ = p;
                break;
            }
        }
    }

    // 6. return node to free list
    freeNode(idx);

    // 7. remove id → index mapping
    id_to_index_[u.order_id] = OrderNode::INVALID_INDEX;
}

void OrderBook::applyUpdate(const MarketUpdate& u) {
    if (u.order_id >= max_orders_) return;

    // Cancel uses node.price (not u.price), so skip the range check for it.
    if (u.type == UpdateType::Cancel) {
        cancelOrder(u);
        return;
    }

    if (u.price < min_price_ || u.price > max_price_) return;

    switch (u.type) {
        case UpdateType::Add:    insertOrder(u); break;
        case UpdateType::Modify: modifyOrder(u); break;
        default: break;
    }
}

bool OrderBook::getBestBid(PriceLevel& out) const {
    // If there is no meaningful best bid, early exit
    if (best_bid_price_ < min_price_) {
        return false;
    }

    int64_t p = best_bid_price_;
    while (p >= min_price_) {
        size_t idx = static_cast<size_t>(p - min_price_);
        const PriceLevel& lvl = bids_[idx];

        if (lvl.head != OrderNode::INVALID_INDEX) {
            out = lvl;           // copy level
            out.price = p;
            return true;
        }
        --p;
    }

    return false; // no bids
}

bool OrderBook::getBestAsk(PriceLevel& out) const {
    // If there is no meaningful best ask, early exit
    if (best_ask_price_ > max_price_) {
        return false;
    }

    int64_t p = best_ask_price_;
    while (p <= max_price_) {
        size_t idx = static_cast<size_t>(p - min_price_);
        const PriceLevel& lvl = asks_[idx];

        if (lvl.head != OrderNode::INVALID_INDEX) {
            out = lvl;           // copy level
            out.price = p;
            return true;
        }
        ++p;
    }

    return false; // no asks
}

