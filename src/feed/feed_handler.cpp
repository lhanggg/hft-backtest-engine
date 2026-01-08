#include "feed_handler.hpp"
#include <iostream>

FeedHandler::FeedHandler(MdQueue& q)
    : _queue(q) {
}

bool FeedHandler::onUpdate(const MarketUpdate& u) {
    //std::cout << "FeedHandler pushing update\n";
    return _queue.push(u);
}

template <typename It>
void FeedHandler::onBatch(It begin, It end) {
    for (auto it = begin; it != end; ++it) {
        const MarketUpdate& u = *it;
        // For now: spin until pushed. Later we can tune this policy.
        while (!_queue.push(u)) {
            // busy-wait; or backoff/yield if needed
        }
    }
}

// If you keep this implementation in the .cpp, you need explicit instantiations
// or move onBatch into the header. Easiest: move the function body into the header.
