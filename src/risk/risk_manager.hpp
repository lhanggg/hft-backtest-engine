#pragma once

#include <cstdint>
#include <cstdlib> // std::abs

#include "engine/strategy_interface.hpp"

// Minimal risk manager: pure stateless checks for Week 7.
// You can extend this later with position tracking, per-symbol limits, etc.
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