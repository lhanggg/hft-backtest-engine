#pragma once
#include <chrono>

static inline std::uint64_t get_monotonic_ns() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}