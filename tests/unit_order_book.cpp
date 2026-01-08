#include "../src/core/order_book.hpp"
#include <cassert>
#include <iostream>

void test_basic_insert() {
    OrderBook ob(90, 110, 1000);

    MarketUpdate u{0, UpdateType::Add, 1, 100, 10, OrderSide::Bid};
    ob.applyUpdate(u);

    PriceLevel pl;
    bool ok = ob.getBestBid(pl);
    assert(ok);
    assert(pl.price == 100);
    assert(pl.total_qty == 10);

    std::cout << "test_basic_insert passed\n";
}

void test_modify_qty() {
    OrderBook ob(90, 110, 1000);

    ob.applyUpdate({0, UpdateType::Add, 1, 100, 10, OrderSide::Bid});
    ob.applyUpdate({0, UpdateType::Modify, 1, 100, 7, OrderSide::Bid});

    PriceLevel pl;
    ob.getBestBid(pl);

    assert(pl.total_qty == 7);
    std::cout << "test_modify_qty passed\n";
}

void test_modify_price() {
    OrderBook ob(90, 110, 1000);

    ob.applyUpdate({0, UpdateType::Add, 1, 100, 10, OrderSide::Bid});
    ob.applyUpdate({0, UpdateType::Modify, 1, 101, 10, OrderSide::Bid});

    PriceLevel pl;
    ob.getBestBid(pl);

    assert(pl.price == 101);
    assert(pl.total_qty == 10);

    std::cout << "test_modify_price passed\n";
}

void test_cancel() {
    OrderBook ob(90, 110, 1000);

    ob.applyUpdate({0, UpdateType::Add, 1, 100, 10, OrderSide::Bid});
    ob.applyUpdate({0, UpdateType::Cancel, 1, 0, 0, OrderSide::Bid});

    PriceLevel pl;
    bool ok = ob.getBestBid(pl);

    assert(!ok);  // book is empty

    std::cout << "test_cancel passed\n";
}

void test_multiple_same_price() {
    OrderBook ob(90, 110, 1000);

    ob.applyUpdate({0, UpdateType::Add, 1, 100, 10, OrderSide::Bid});
    ob.applyUpdate({0, UpdateType::Add, 2, 100, 5,  OrderSide::Bid});

    PriceLevel pl;
    ob.getBestBid(pl);

    assert(pl.total_qty == 15);
    std::cout << "test_multiple_same_price passed\n";
}

void test_node_recycling() {
    OrderBook ob(90, 110, 3);

    ob.applyUpdate({0, UpdateType::Add, 1, 100, 10, OrderSide::Bid});
    ob.applyUpdate({0, UpdateType::Cancel, 1, 0, 0, OrderSide::Bid});

    // This should reuse the same node index
    ob.applyUpdate({0, UpdateType::Add, 2, 101, 5, OrderSide::Bid});

    PriceLevel pl;
    ob.getBestBid(pl);

    assert(pl.price == 101);
    assert(pl.total_qty == 5);

    std::cout << "test_node_recycling passed\n";
}

int main() {
    test_basic_insert();
    test_modify_qty();
    test_modify_price();
    test_cancel();
    test_multiple_same_price();
    test_node_recycling();

    std::cout << "All order book tests passed\n";
    return 0;
}