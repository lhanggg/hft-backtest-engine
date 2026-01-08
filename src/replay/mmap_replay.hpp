#pragma once

#include <cstdint>

class FeedHandler;

std::uint64_t run_mmap_replay(FeedHandler& fh, const char* filename);
