#include "binary_parser.hpp"
#include <cstring> 


size_t BinaryParser::parse(const uint8_t* begin,
                                const uint8_t* end,
                                MarketUpdate& out)
{
    // TEMPORARY STUB FOR WEEK 5
    // Replace with real binary decoding later.

    if (begin + sizeof(MarketUpdate) > end) {
        return 0; // not enough bytes
    }

    // Just memcpy for now (assuming your feed file is raw MarketUpdate structs)
    std::memcpy(&out, begin, sizeof(MarketUpdate));
    return sizeof(MarketUpdate);
}