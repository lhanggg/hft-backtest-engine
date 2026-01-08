#pragma once

#include <cstddef>
#include "../core/order_book.hpp"
#include "../core/ring_buffer.hpp"

using MdQueue = SpscRing<MarketUpdate>;

class FeedHandler {
public:
    explicit FeedHandler(MdQueue& queue);

    // Called by replay or network code for each decoded update.
    // Returns false if the queue is full (caller decides what to do).
    bool onUpdate(const MarketUpdate& u);

    // Optional: batch helper for replay
    template <typename It>
    void onBatch(It begin, It end);

private:
    MdQueue& _queue;
};
