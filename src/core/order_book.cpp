#include "order_book.hpp"

#include <iostream>

OrderBook::OrderBook(int64_t min_price,
                     int64_t max_price,
                     size_t  max_orders)
    : _min_price(min_price),
      _max_price(max_price),
      _max_orders(max_orders),
      _best_bid_price(min_price),
      _best_ask_price(max_price)
{
    _id_to_index.resize(_max_orders, OrderNode::INVALID_INDEX);

    _num_levels = static_cast<size_t>(_max_price - _min_price + 1);

    // allocate price levels
    _bids = new PriceLevel[_num_levels];
    _asks = new PriceLevel[_num_levels];

    // allocate nodes
    _nodes = new OrderNode[_max_orders];
    // initialize free list
    _free_head = 0;
    for (size_t i = 0; i < _max_orders; ++i) {
        _nodes[i].next = (i + 1 < _max_orders) ? i + 1 : OrderNode::INVALID_INDEX;
    }
}

uint32_t OrderBook::allocNode() {
    if (_free_head == OrderNode::INVALID_INDEX) {
        // out of memory 
        return OrderNode::INVALID_INDEX;
    }

    uint32_t idx = _free_head;
    _free_head = _nodes[idx].next;   // move head to next free node
    return idx;
}

void OrderBook::freeNode(uint32_t idx) {
    _nodes[idx].next = _free_head;   // push this node onto the free list
    _free_head = idx;
}

void OrderBook::insertOrder(const MarketUpdate& u) {
    // 1. allocate node
    uint32_t idx = allocNode();
    if (idx == OrderNode::INVALID_INDEX) return;

    // 2. fill node fields
    OrderNode& node = _nodes[idx];
    node.order_id = u.order_id;
    node.price    = u.price;
    node.qty      = u.qty;
    node.side     = u.side;
    node.next     = OrderNode::INVALID_INDEX;

    // 3. map price → level index
    size_t level_idx = static_cast<size_t>(u.price - _min_price);

    PriceLevel* levels = (u.side == OrderSide::Bid) ? _bids : _asks;
    PriceLevel& level  = levels[level_idx];

    // 4. insert into price level (FIFO)
    if (level.head == OrderNode::INVALID_INDEX) {
        // empty level
        level.head = idx;
        level.tail = idx;
        level.price = u.price;
    } else {
        // append to tail
        _nodes[level.tail].next = idx;
        level.tail = idx;
    }

    // 5. update total quantity
    level.total_qty += u.qty;

    // 6. update best bid/ask
    if (u.side == OrderSide::Bid) {
        if (u.price > _best_bid_price) {
            _best_bid_price = u.price;
        }
    } else {
        if (u.price < _best_ask_price) {
            _best_ask_price = u.price;
        }
    }

    // 7. store id → index mapping
    _id_to_index[u.order_id] = idx;

}

void OrderBook::modifyOrder(const MarketUpdate& u) {
    // 1. find node
    uint32_t idx = _id_to_index[u.order_id];
    if (idx == OrderNode::INVALID_INDEX) return;

    OrderNode& node = _nodes[idx];
    // 2. quantity-only modify
    if (u.price == node.price) {
        int32_t delta = u.qty - node.qty;
        node.qty = u.qty;

        size_t level_idx = static_cast<size_t>(node.price - _min_price);
        PriceLevel* levels = (node.side == OrderSide::Bid) ? _bids : _asks;
        PriceLevel& level = levels[level_idx];

        level.total_qty += delta;
        return;
    }

    // 3. price change: remove from old level
    size_t old_idx = static_cast<size_t>(node.price - _min_price);
    PriceLevel* old_levels = (node.side == OrderSide::Bid) ? _bids : _asks;
    PriceLevel& old_level = old_levels[old_idx];

    uint32_t prev = OrderNode::INVALID_INDEX;
    uint32_t cur = old_level.head;

    while (cur != OrderNode::INVALID_INDEX) {
        if (cur == idx) break;
        prev = cur;
        cur = _nodes[cur].next;
    }

    if (cur == OrderNode::INVALID_INDEX) return; // should not happen

    // unlink
    if (prev == OrderNode::INVALID_INDEX) {
        // removing head
        old_level.head = _nodes[cur].next;
    } else {
        _nodes[prev].next = _nodes[cur].next;
    }

    if (old_level.tail == idx) {
        old_level.tail = prev;
    }

    old_level.total_qty -= node.qty;

    // 4. update best price if needed
    if (node.side == OrderSide::Bid && node.price == _best_bid_price) {
        // scan downward to find next non-empty bid
        for (int64_t p = _best_bid_price; p >= _min_price; --p) {
            size_t li = static_cast<size_t>(p - _min_price);
            if (_bids[li].head != OrderNode::INVALID_INDEX) {
                _best_bid_price = p;
                break;
            }
        }
    }

    if (node.side == OrderSide::Ask && node.price == _best_ask_price) {
        // scan upward to find next non-empty ask
        for (int64_t p = _best_ask_price; p <= _max_price; ++p) {
            size_t li = static_cast<size_t>(p - _min_price);
            if (_asks[li].head != OrderNode::INVALID_INDEX) {
                _best_ask_price = p;
                break;
            }
        }
    }

    // 5. update node fields
    node.price = u.price;
    node.qty   = u.qty;
    node.next  = OrderNode::INVALID_INDEX;

    // 6. insert into new price level
    size_t new_idx = static_cast<size_t>(u.price - _min_price);
    PriceLevel* new_levels = (node.side == OrderSide::Bid) ? _bids : _asks;
    PriceLevel& new_level = new_levels[new_idx];

    if (new_level.head == OrderNode::INVALID_INDEX) {
        new_level.head = idx;
        new_level.tail = idx;
    } else {
        _nodes[new_level.tail].next = idx;
        new_level.tail = idx;
    }

    new_level.total_qty += node.qty;

    // 7. update best price for new level
    if (node.side == OrderSide::Bid && u.price > _best_bid_price) {
        _best_bid_price = u.price;
    }
    if (node.side == OrderSide::Ask && u.price < _best_ask_price) {
        _best_ask_price = u.price;
    }
}

void OrderBook::cancelOrder(const MarketUpdate& u) {
    // 1. find node
    uint32_t idx = _id_to_index[u.order_id];
    if (idx == OrderNode::INVALID_INDEX) return;

    OrderNode& node = _nodes[idx];
    // 2. find price level
    size_t level_idx = static_cast<size_t>(node.price - _min_price);
    PriceLevel* levels = (node.side == OrderSide::Bid) ? _bids : _asks;
    PriceLevel& level = levels[level_idx];

    // 3. unlink node from list
    uint32_t prev = OrderNode::INVALID_INDEX;
    uint32_t cur = level.head;

    while (cur != OrderNode::INVALID_INDEX) {
        if (cur == idx) break;
        prev = cur;
        cur = _nodes[cur].next;
    }

    if (cur == OrderNode::INVALID_INDEX) return; // should not happen

    if (prev == OrderNode::INVALID_INDEX) {
        // removing head
        level.head = _nodes[cur].next;
    } else {
        _nodes[prev].next = _nodes[cur].next;
    }

    if (level.tail == idx) {
        level.tail = prev;
    }

    // 4. update total quantity
    level.total_qty -= node.qty;

    // 5. update best price if needed
    if (node.side == OrderSide::Bid && node.price == _best_bid_price) {
        for (int64_t p = _best_bid_price; p >= _min_price; --p) {
            size_t li = static_cast<size_t>(p - _min_price);
            if (_bids[li].head != OrderNode::INVALID_INDEX) {
                _best_bid_price = p;
                break;
            }
        }
    }

    if (node.side == OrderSide::Ask && node.price == _best_ask_price) {
        for (int64_t p = _best_ask_price; p <= _max_price; ++p) {
            size_t li = static_cast<size_t>(p - _min_price);
            if (_asks[li].head != OrderNode::INVALID_INDEX) {
                _best_ask_price = p;
                break;
            }
        }
    }

    // 6. return node to free list
    freeNode(idx);

    // 7. remove id → index mapping
    _id_to_index[u.order_id] = OrderNode::INVALID_INDEX;
}

void OrderBook::applyUpdate(const MarketUpdate& u) {
    if (u.price < _min_price || u.price > _max_price) {
        // Ignore out-of-range prices for this simple benchmark
        return;
    }

    if (u.order_id >= _max_orders) {
        // Ignore absurd order_id for now
        return;
    }

    switch (u.type) {
        case UpdateType::Add:    insertOrder(u);   break;
        case UpdateType::Modify: modifyOrder(u);   break;
        case UpdateType::Cancel: cancelOrder(u);   break;
        default: break;
    }
}

bool OrderBook::getBestBid(PriceLevel& out) const {
    // If there is no meaningful best bid, early exit
    if (_best_bid_price < _min_price) {
        return false;
    }

    int64_t p = _best_bid_price;
    while (p >= _min_price) {
        size_t idx = static_cast<size_t>(p - _min_price);
        const PriceLevel& lvl = _bids[idx];

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
    if (_best_ask_price > _max_price) {
        return false;
    }

    int64_t p = _best_ask_price;
    while (p <= _max_price) {
        size_t idx = static_cast<size_t>(p - _min_price);
        const PriceLevel& lvl = _asks[idx];

        if (lvl.head != OrderNode::INVALID_INDEX) {
            out = lvl;           // copy level
            out.price = p;
            return true;
        }
        ++p;
    }

    return false; // no asks
}

