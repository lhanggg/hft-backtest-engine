# Trading System — README

A lightweight, allocation-free backtesting / replay engine developed on Windows (MinGW/CMake).

## Typical workflow
1. Generate a feed file:
   ```sh
   build/generate_feed.exe feed.bin 1000000
   ```
   (See [`src/tools/generate_feed.cpp`](src/tools/generate_feed.cpp))

2. Replay feed via memory-map and handler (concurrent producer/consumer with thread affinity):
   ```sh
   build/feed_throughput.exe feed.bin                  # no affinity — OS schedules
   build/feed_throughput.exe feed.bin 4 5              # pin to specific cores (HT pair = fastest)
   ```
   Replay implementation: [`run_mmap_replay`](src/replay/mmap_replay.cpp) which calls [`BinaryParser::parse`](src/feed/binary_parser.cpp).

3. Run unit tests and integration tests:
   ```sh
   ctest --test-dir build --output-on-failure
   ```
   Tests: [`tests/unit_order_book.cpp`](tests/unit_order_book.cpp), [`tests/unit_ring_buffer.cpp`](tests/unit_ring_buffer.cpp), [`tests/integration_event_loop.cpp`](tests/integration_event_loop.cpp).

4. Run order book microbenchmarks:
   ```sh
   build/bench_order_book.exe
   ```
   See [`benchmarks/bench_order_book.cpp`](benchmarks/bench_order_book.cpp).

5. Run the imbalance strategy backtest:
   ```sh
   build/run_backtest.exe feed.bin              # defaults: alpha=0.1, threshold=0.3
   build/run_backtest.exe feed.bin 0.1 0.05     # lower threshold → more signals
   ```
   See [`src/tools/run_backtest.cpp`](src/tools/run_backtest.cpp) and [`src/engine/imbalance_strategy.hpp`](src/engine/imbalance_strategy.hpp).

## Key components
- Build configuration: [CMakeLists.txt](CMakeLists.txt)
- Replay / mmap ingest: [src/replay/mmap_replay.cpp](src/replay/mmap_replay.cpp) — `run_mmap_replay`
- Feed parsing: [src/feed/binary_parser.cpp](src/feed/binary_parser.cpp) — `BinaryParser::parse`
- SPSC ring buffer: [src/core/ring_buffer.hpp](src/core/ring_buffer.hpp) — `SpscRing`
- Market model / order book: [src/core/market_data.hpp](src/core/market_data.hpp), [src/core/order_book.hpp](src/core/order_book.hpp) — `OrderBook`
- Engine loop: [src/engine/event_loop.cpp](src/engine/event_loop.cpp) — `EventLoop::run`
- Example strategy: [src/engine/strategy_example.cpp](src/engine/strategy_example.cpp) — `DummyStrategy`
- Order book microbench: [benchmarks/bench_order_book.cpp](benchmarks/bench_order_book.cpp)
- Feed throughput bench: [benchmarks/feed_throughput.cpp](benchmarks/feed_throughput.cpp)
- Imbalance strategy: [src/engine/imbalance_strategy.hpp](src/engine/imbalance_strategy.hpp) — EMA-based order book imbalance signal with PnL tracking
- Backtest runner: [src/tools/run_backtest.cpp](src/tools/run_backtest.cpp)

## Benchmarks

Run `build/bench_order_book.exe` to reproduce. Measured with `__rdtsc` (TSC calibrated against `steady_clock`), 1M samples, i7-12700H @2.69 GHz, Windows, 50K warmup iterations.

```
benchmark           p50        p99       p99.9        max
---------         ------     ------     -------     ------
getBestBid        10.4 ns    17.1 ns    33.5 ns    157345.3 ns
insert            16.4 ns    22.3 ns    57.3 ns    140932.4 ns
modify-qty        14.1 ns    18.6 ns    32.0 ns    178076.7 ns
cancel            16.4 ns    26.8 ns    43.2 ns    166420.5 ns
```

`OrderNode` uses a doubly-linked list (`prev` + `next` indices) for O(1) unlink on cancel/modify.
Max values reflect OS scheduler jitter; the hot-path numbers are p50/p99/p99.9.

### SPSC feed throughput (concurrent producer + consumer, 1M msgs, i7-12700H, Windows)

```
cores (producer, consumer)   type                         throughput
--------------------------   ----                         ----------
4, 5                         HT pair (same physical core) 17.6 M msgs/sec  ← best
4, 6                         separate physical P-cores    7.9 M msgs/sec
none (OS schedules)          unpinned                     4.4 M msgs/sec   ← worst
```

**Key finding:** pinning producer and consumer to the same physical core's hyperthreads (cores 4/5)
is **4x faster** than unpinned and **2x faster** than separate physical cores.
The ring buffer fits in shared L1/L2 cache — no coherency traffic needed between cores.
Unpinned threads are migrated by the OS mid-run, causing random cache misses and context switches.
Run `feed_throughput.exe feed.bin <producer_core> <consumer_core>` to reproduce.

### Backtest results (ImbalanceStrategy, 1M msgs, i7-12700H, Windows)

```
feed        : 1 000 000 random orders, prices 10000 ± 50
throughput  : ~29 M updates/sec (single-threaded event loop)
```

| alpha | threshold | signals | round trips | realized PnL (ticks) |
|-------|-----------|---------|-------------|----------------------|
| 0.1   | 0.30      | 2       | 1           | 97                   |
| 0.1   | 0.05      | 5       | 4           | 283                  |

Signal: order-book imbalance EMA crosses ±threshold → market order at best ask/bid.
PnL is in price ticks (mark-to-market); random feed so values are noise by design.
Run `run_backtest.exe feed.bin <alpha> <threshold>` to reproduce.

## Notes & tips
- Project targets MinGW; toolchain detected in build artifacts (see `build/` and `build/compile_commands.json`).
- Binaries produced in `build/` (examples: `generate_feed.exe`, `feed_throughput.exe`, `bench_order_book.exe`).
- Design goals: allocation-free hot path, SPSC rings for handoff, mmap replay for zero-copy parsing (see `design.md`).
