#pragma once

#include <cstdint>
#include <cstdlib> // std::abs

#include "engine/strategy_interface.hpp"

// Stateless pre-trade risk checks: price and quantity bounds.
class RiskManager {
public:
    RiskManager(std::int64_t max_abs_price, std::int64_t max_abs_qty)
        : max_abs_price_(max_abs_price)
        , max_abs_qty_(max_abs_qty) {}

    bool check(const StrategySignal& sig) const {
        if (std::llabs(sig.qty) > max_abs_qty_)   return false;
        if (std::llabs(sig.price) > max_abs_price_) return false;
        return true;
    }

private:
    std::int64_t max_abs_price_;
    std::int64_t max_abs_qty_;
};