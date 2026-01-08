#include <cstdint>
#include "../core/market_data.hpp"

struct BinaryParser {
    // Returns number of bytes consumed, 0 on error/stop.
    size_t parse(const uint8_t* begin,
                      const uint8_t* end,
                      MarketUpdate& out);
};
