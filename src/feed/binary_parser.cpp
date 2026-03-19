#include "binary_parser.hpp"
#include <cstring> 


size_t BinaryParser::parse(const uint8_t* begin,
                                const uint8_t* end,
                                MarketUpdate& out)
{
    if (begin + sizeof(MarketUpdate) > end) {
        return 0; // not enough bytes
    }

    // Feed file is raw MarketUpdate structs written by generate_feed
    std::memcpy(&out, begin, sizeof(MarketUpdate));
    return sizeof(MarketUpdate);
}