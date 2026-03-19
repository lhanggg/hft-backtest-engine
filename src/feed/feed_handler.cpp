#include "feed_handler.hpp"
#include <iostream>

FeedHandler::FeedHandler(MdQueue& q)
    : queue_(q) {
}

bool FeedHandler::onUpdate(const MarketUpdate& u) {
    return queue_.push(u);
}

template <typename It>
void FeedHandler::onBatch(It begin, It end) {
    for (auto it = begin; it != end; ++it) {
        const MarketUpdate& u = *it;
        while (!queue_.push(u)) {}
    }
}
