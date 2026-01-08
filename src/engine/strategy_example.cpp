#include "engine/strategy_interface.hpp"
#include "core/order_book.hpp"

#include "core/ring_buffer.hpp"
#include <iostream>

// A simple example strategy:
// - counts market updates
// - every N updates, emits a tiny signal using the last price
class DummyStrategy : public Strategy {
public:
    DummyStrategy(SpscRing<StrategySignal>& out_queue, std::uint32_t every_n)
        : out_queue_(out_queue)
        , every_n_(every_n) {}

    void on_market_update(const MarketUpdate& mu) override {
        ++count_;
        last_price_ = mu.price;

        if (count_ % every_n_ == 0) {
            StrategySignal s;
            s.price = last_price_;
            s.qty   = 1;
            out_queue_.push(s);
        }
    }

    void on_timer(std::uint64_t ts) override 
    {
        ++count_;

        if (count_ % every_n_ == 0) {
            StrategySignal sig;
            sig.price = last_price_;
            sig.qty = 1;
            out_queue_.push(sig);
            std::cout << "Strategy emitted signal: price=" << sig.price
                    << " qty=" << sig.qty << "\n";
        }
    }

    bool poll_signal(StrategySignal& out) override {
        if (!has_pending_) return false;
        out = pending_;
        has_pending_ = false;
        return true;
    }

private:
    SpscRing<StrategySignal>& out_queue_;
    std::uint32_t              every_n_;
    std::uint64_t              count_      = 0;
    std::int64_t               last_price_ = 0;

    // Only used if you choose the poll_signal() model:
    StrategySignal pending_{};
    bool           has_pending_ = false;
};