#pragma once

#include "engine/strategy_interface.hpp"
#include "core/order_book.hpp"

#include <cstdint>
#include <iostream>

// ---------------------------------------------------------------------------
// ImbalanceStrategy
//
// Signal: order-book imbalance at best bid/ask =
//     (bid_qty - ask_qty) / (bid_qty + ask_qty)   ∈ [-1, +1]
//
// An EMA of the raw imbalance is maintained each tick (configurable alpha).
//
// Rules:
//   EMA > +threshold → buy  at best ask (if not already long)
//   EMA < -threshold → sell at best bid (if not already short)
//
// PnL tracking (in price ticks, mark-to-market):
//   Fills are assumed immediate at the signal price.
//   A new signal in the opposite direction closes the current position first.
// ---------------------------------------------------------------------------
class ImbalanceStrategy : public Strategy {
public:
    ImbalanceStrategy(const OrderBook& ob,
                      double           ema_alpha  = 0.1,
                      double           threshold  = 0.3)
        : ob_(ob)
        , alpha_(ema_alpha)
        , threshold_(threshold)
    {}

    // Called after order_book_.applyUpdate() — book is already current.
    void on_market_update(const MarketUpdate&) override {
        PriceLevel bid, ask;
        if (!ob_.getBestBid(bid) || !ob_.getBestAsk(ask)) return;
        if (bid.total_qty + ask.total_qty == 0) return;

        // Raw imbalance ∈ [-1, +1]
        double raw = (double)(bid.total_qty - ask.total_qty)
                   / (double)(bid.total_qty + ask.total_qty);

        // EMA update
        ema_ = alpha_ * raw + (1.0 - alpha_) * ema_;

        int64_t mid = (bid.price + ask.price) / 2;

        // Mark open position to market
        if (position_ != 0) {
            unrealized_pnl_ = (double)(mid - entry_price_) * position_;
        }

        // Buy signal: EMA strongly positive → price likely to rise
        if (ema_ > threshold_ && last_signal_ != 1) {
            close_position(ask.price);   // close any open short
            position_    = 1;
            entry_price_ = ask.price;
            last_signal_ = 1;
            ++signals_emitted_;
            pending_     = {ask.price, 1};
            has_pending_ = true;
        }
        // Sell signal: EMA strongly negative → price likely to fall
        else if (ema_ < -threshold_ && last_signal_ != -1) {
            close_position(bid.price);   // close any open long
            position_    = -1;
            entry_price_ = bid.price;
            last_signal_ = -1;
            ++signals_emitted_;
            pending_     = {bid.price, -1};
            has_pending_ = true;
        }

        ++ticks_;
    }

    void on_timer(std::uint64_t) override {
        print_summary();
    }

    bool poll_signal(StrategySignal& out) override {
        if (!has_pending_) return false;
        out = pending_;
        has_pending_ = false;
        return true;
    }

    void print_summary() const {
        // Close position at current mid for final PnL
        double final_pnl = realized_pnl_;
        PriceLevel bid, ask;
        if (position_ != 0 && ob_.getBestBid(bid) && ob_.getBestAsk(ask)) {
            int64_t mid = (bid.price + ask.price) / 2;
            final_pnl += (double)(mid - entry_price_) * position_;
        }
        std::cout << "[ImbalanceStrategy]"
                  << "  ticks="        << ticks_
                  << "  signals="      << signals_emitted_
                  << "  round_trips="  << round_trips_
                  << "  realized_pnl=" << realized_pnl_
                  << "  total_pnl="    << final_pnl
                  << "  ema="          << ema_
                  << "\n";
    }

    // Accessors for tests / tools
    double   ema()            const { return ema_; }
    int64_t  signals_emitted()const { return (int64_t)signals_emitted_; }
    double   realized_pnl()   const { return realized_pnl_; }
    uint64_t ticks()          const { return ticks_; }

private:
    void close_position(int64_t close_price) {
        if (position_ == 0) return;
        realized_pnl_ += (double)(close_price - entry_price_) * position_;
        unrealized_pnl_ = 0.0;
        position_       = 0;
        entry_price_    = 0;
        ++round_trips_;
    }

    const OrderBook& ob_;
    double           alpha_;
    double           threshold_;

    double   ema_             = 0.0;
    int      last_signal_     = 0;    // +1 last buy, -1 last sell, 0 none
    int      position_        = 0;    // +1 long, -1 short, 0 flat
    int64_t  entry_price_     = 0;

    double   realized_pnl_    = 0.0;
    double   unrealized_pnl_  = 0.0;

    uint64_t ticks_           = 0;
    uint64_t signals_emitted_ = 0;
    uint64_t round_trips_     = 0;

    StrategySignal pending_{};
    bool           has_pending_ = false;
};
