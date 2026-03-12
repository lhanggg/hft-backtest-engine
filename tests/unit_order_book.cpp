#include "../src/core/order_book.hpp"
#include <cassert>
#include <iostream>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static MarketUpdate add(uint64_t id, int64_t price, int32_t qty, OrderSide side) {
    return {0, UpdateType::Add, id, price, qty, side};
}
static MarketUpdate modify(uint64_t id, int64_t price, int32_t qty, OrderSide side) {
    return {0, UpdateType::Modify, id, price, qty, side};
}
static MarketUpdate cancel(uint64_t id) {
    return {0, UpdateType::Cancel, id, 0, 0, OrderSide::Bid};
}

// ---------------------------------------------------------------------------
// Bid side — insert / getBestBid
// ---------------------------------------------------------------------------
void test_basic_insert() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.price == 100);
    assert(pl.total_qty == 10);
    std::cout << "test_basic_insert passed\n";
}

void test_best_bid_multiple_levels() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1,  98, 5,  OrderSide::Bid));
    ob.applyUpdate(add(2, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(3,  99, 7,  OrderSide::Bid));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.price == 100);
    assert(pl.total_qty == 10);
    std::cout << "test_best_bid_multiple_levels passed\n";
}

// ---------------------------------------------------------------------------
// Ask side — insert / getBestAsk
// ---------------------------------------------------------------------------
void test_basic_insert_ask() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 102, 8, OrderSide::Ask));

    PriceLevel pl;
    assert(ob.getBestAsk(pl));
    assert(pl.price == 102);
    assert(pl.total_qty == 8);
    std::cout << "test_basic_insert_ask passed\n";
}

void test_best_ask_multiple_levels() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 105, 3, OrderSide::Ask));
    ob.applyUpdate(add(2, 101, 6, OrderSide::Ask));
    ob.applyUpdate(add(3, 103, 9, OrderSide::Ask));

    PriceLevel pl;
    assert(ob.getBestAsk(pl));
    assert(pl.price == 101);
    assert(pl.total_qty == 6);
    std::cout << "test_best_ask_multiple_levels passed\n";
}

// ---------------------------------------------------------------------------
// Empty book
// ---------------------------------------------------------------------------
void test_empty_book() {
    OrderBook ob(90, 110, 1000);
    PriceLevel pl;
    assert(!ob.getBestBid(pl));
    assert(!ob.getBestAsk(pl));
    std::cout << "test_empty_book passed\n";
}

// ---------------------------------------------------------------------------
// Modify — qty only (same price, fast path)
// ---------------------------------------------------------------------------
void test_modify_qty() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(modify(1, 100, 7, OrderSide::Bid));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.total_qty == 7);
    std::cout << "test_modify_qty passed\n";
}

void test_modify_qty_ask() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 103, 12, OrderSide::Ask));
    ob.applyUpdate(modify(1, 103, 5, OrderSide::Ask));

    PriceLevel pl;
    assert(ob.getBestAsk(pl));
    assert(pl.total_qty == 5);
    std::cout << "test_modify_qty_ask passed\n";
}

// ---------------------------------------------------------------------------
// Modify — price change
// ---------------------------------------------------------------------------
void test_modify_price() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(modify(1, 101, 10, OrderSide::Bid));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.price == 101);
    assert(pl.total_qty == 10);
    std::cout << "test_modify_price passed\n";
}

void test_modify_price_old_level_emptied() {
    // After moving the only order off a level, best price should update
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(2,  98,  5, OrderSide::Bid));
    ob.applyUpdate(modify(1, 99, 10, OrderSide::Bid));  // 100 is now empty

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.price == 99);   // 100 was vacated, 99 is new best
    std::cout << "test_modify_price_old_level_emptied passed\n";
}

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------
void test_cancel_only_order() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(cancel(1));

    PriceLevel pl;
    assert(!ob.getBestBid(pl));
    std::cout << "test_cancel_only_order passed\n";
}

void test_cancel_updates_total_qty() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(2, 100,  5, OrderSide::Bid));
    ob.applyUpdate(cancel(1));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.price == 100);
    assert(pl.total_qty == 5);
    std::cout << "test_cancel_updates_total_qty passed\n";
}

void test_cancel_best_bid_updates_price() {
    // Cancelling the best level should expose the next level
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(2,  98,  5, OrderSide::Bid));
    ob.applyUpdate(cancel(1));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.price == 98);
    assert(pl.total_qty == 5);
    std::cout << "test_cancel_best_bid_updates_price passed\n";
}

void test_cancel_best_ask_updates_price() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 101, 10, OrderSide::Ask));
    ob.applyUpdate(add(2, 103,  5, OrderSide::Ask));
    ob.applyUpdate(cancel(1));

    PriceLevel pl;
    assert(ob.getBestAsk(pl));
    assert(pl.price == 103);
    assert(pl.total_qty == 5);
    std::cout << "test_cancel_best_ask_updates_price passed\n";
}

// ---------------------------------------------------------------------------
// Cancel — head / middle / tail unlink (exercises doubly-linked list)
// ---------------------------------------------------------------------------
void test_cancel_head_node() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(2, 100,  5, OrderSide::Bid));
    ob.applyUpdate(add(3, 100,  3, OrderSide::Bid));
    ob.applyUpdate(cancel(1));  // cancel head

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.total_qty == 8);  // 5 + 3
    std::cout << "test_cancel_head_node passed\n";
}

void test_cancel_middle_node() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(2, 100,  5, OrderSide::Bid));
    ob.applyUpdate(add(3, 100,  3, OrderSide::Bid));
    ob.applyUpdate(cancel(2));  // cancel middle

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.total_qty == 13);  // 10 + 3
    std::cout << "test_cancel_middle_node passed\n";
}

void test_cancel_tail_node() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(2, 100,  5, OrderSide::Bid));
    ob.applyUpdate(add(3, 100,  3, OrderSide::Bid));
    ob.applyUpdate(cancel(3));  // cancel tail

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.total_qty == 15);  // 10 + 5
    std::cout << "test_cancel_tail_node passed\n";
}

// ---------------------------------------------------------------------------
// Multiple orders at same price — aggregated qty
// ---------------------------------------------------------------------------
void test_multiple_same_price() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(add(2, 100,  5, OrderSide::Bid));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.total_qty == 15);
    std::cout << "test_multiple_same_price passed\n";
}

// ---------------------------------------------------------------------------
// Node free-list recycling
// ---------------------------------------------------------------------------
void test_node_recycling() {
    OrderBook ob(90, 110, 3);
    ob.applyUpdate(add(1, 100, 10, OrderSide::Bid));
    ob.applyUpdate(cancel(1));
    ob.applyUpdate(add(2, 101,  5, OrderSide::Bid));

    PriceLevel pl;
    assert(ob.getBestBid(pl));
    assert(pl.price == 101);
    assert(pl.total_qty == 5);
    std::cout << "test_node_recycling passed\n";
}

// ---------------------------------------------------------------------------
// Robustness — out-of-range price and unknown order_id should not crash
// ---------------------------------------------------------------------------
void test_out_of_range_price_ignored() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(add(1, 50, 10, OrderSide::Bid));   // price < min_price

    PriceLevel pl;
    assert(!ob.getBestBid(pl));  // nothing was inserted
    std::cout << "test_out_of_range_price_ignored passed\n";
}

void test_cancel_nonexistent_order() {
    OrderBook ob(90, 110, 1000);
    ob.applyUpdate(cancel(42));  // should be a no-op, not a crash
    std::cout << "test_cancel_nonexistent_order passed\n";
}

// ---------------------------------------------------------------------------
int main() {
    test_basic_insert();
    test_best_bid_multiple_levels();
    test_basic_insert_ask();
    test_best_ask_multiple_levels();
    test_empty_book();

    test_modify_qty();
    test_modify_qty_ask();
    test_modify_price();
    test_modify_price_old_level_emptied();

    test_cancel_only_order();
    test_cancel_updates_total_qty();
    test_cancel_best_bid_updates_price();
    test_cancel_best_ask_updates_price();

    test_cancel_head_node();
    test_cancel_middle_node();
    test_cancel_tail_node();

    test_multiple_same_price();
    test_node_recycling();
    test_out_of_range_price_ignored();
    test_cancel_nonexistent_order();

    std::cout << "\nAll order book tests passed\n";
    return 0;
}
