# Trading System — README

A lightweight, allocation-free backtesting / replay engine developed on Windows (MinGW/CMake).

## Typical workflow
1. Generate a feed file:
   ```sh
   build/generate_feed.exe feed.bin 1000000
   ```
   (See [`src/tools/generate_feed.cpp`](src/tools/generate_feed.cpp))

2. Replay feed via memory-map and handler:
   ```sh
   build/feed_throughput.exe feed.bin
   ```
   Replay implementation: [`run_mmap_replay`](src/replay/mmap_replay.cpp) which calls [`BinaryParser::parse`](src/feed/binary_parser.cpp).
3. Test event loops and get preformance result
   ```sh
   build/integration_event_loop.exe
   ```
   
## Key components
- Build configuration: [CMakeLists.txt](CMakeLists.txt)  
- Replay / mmap ingest: [src/replay/mmap_replay.cpp](src/replay/mmap_replay.cpp) — [`run_mmap_replay`](src/replay/mmap_replay.cpp)  
- Feed parsing: [src/feed/binary_parser.cpp](src/feed/binary_parser.cpp) — [`BinaryParser::parse`](src/feed/binary_parser.cpp)  
- SPSC ring buffer: [src/core/ring_buffer.hpp](src/core/ring_buffer.hpp) — `SpscRing`  
- Market model / order book: [src/core/market_data.hpp](src/core/market_data.hpp), [src/core/order_book.hpp](src/core/order_book.hpp)  
- Engine loop: [src/engine/event_loop.cpp](src/engine/event_loop.cpp) — [`EventLoop::run`](src/engine/event_loop.cpp)  
- Example strategy: [src/engine/strategy_example.cpp](src/engine/strategy_example.cpp) — `DummyStrategy`  

## Notes & tips
- Project targets MinGW; toolchain detected in build artifacts (see `build/` and `build/compile_commands.json`).  
- Binaries produced in `build/` (examples: `generate_feed.exe`, `feed_throughput.exe`, `integration_event_loop.exe`).  
- Design goals: allocation-free hot path, SPSC rings for handoff, mmap replay for zero-copy parsing (see `design.md`).
