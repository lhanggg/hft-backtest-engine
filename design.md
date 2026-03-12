# Project Design — Backtesting Engine

This document summarizes architecture, data layouts, APIs, and operational notes.

## Goals
- Ultra-low-latency event-driven backtester: target 1M+ updates/sec with sub-microsecond processing on the hot path.
- Allocation-free hot paths, lock-free handoff, deterministic replay, and reproducible profiling.

## High-level Architecture

```
Feed file (mmap)
    → BinaryParser
    → FeedHandler
    → SpscRing (SPSC queue)
    → EventLoop
        → OrderBook::applyUpdate
        → Strategy::on_market_update
        → RiskManager::check
        → execution / logging
```

Key components:
- Producer / parser: [`BinaryParser::parse`](src/feed/binary_parser.cpp) and generator [`src/tools/generate_feed.cpp`](src/tools/generate_feed.cpp).
- Replay/mmap ingestion: [`run_mmap_replay`](src/replay/mmap_replay.cpp) maps feed files and feeds the handler.
- Feed handler: [`FeedHandler`](src/feed/feed_handler.hpp).
- Ring buffers: [`SpscRing`](src/core/ring_buffer.hpp).
- Market model: [`MarketUpdate`](src/core/market_data.hpp) and [`OrderBook`](src/core/order_book.hpp).
- Engine: [`EventLoop`](src/engine/event_loop.cpp) and [`DummyStrategy`](src/engine/strategy_example.cpp).
- Risk: [`RiskManager`](src/risk/risk_manager.hpp).

## Data Layouts

### Feed record
32-byte packed record written by [`src/tools/generate_feed.cpp`](src/tools/generate_feed.cpp); replayed zero-copy via mmap in [`src/replay/mmap_replay.cpp`](src/replay/mmap_replay.cpp).

### MarketUpdate
Small POD passed through the SPSC ring — see [`src/core/market_data.hpp`](src/core/market_data.hpp).

### OrderBook internals
See [`src/core/order_book.hpp`](src/core/order_book.hpp) and [`src/core/order_book.cpp`](src/core/order_book.cpp).

**`OrderNode`** — 64-byte cache-line aligned:
| Field      | Type       | Notes                                  |
|------------|------------|----------------------------------------|
| `order_id` | `uint64_t` | unique order identifier                |
| `price`    | `int64_t`  | price in ticks                         |
| `qty`      | `int32_t`  | remaining quantity                     |
| `next`     | `uint32_t` | index of next node, or `INVALID_INDEX` |
| `prev`     | `uint32_t` | index of prev node, or `INVALID_INDEX` |
| `side`     | `OrderSide`| bid or ask                             |
| `_pad`     | `uint8_t[35]` | padding to 64 bytes                 |

Nodes are stored in a flat array (`nodes_[]`). A free-list (`free_head_`) provides O(1) alloc/free without heap calls on the hot path. `id_to_index_[]` maps `order_id → node index` for O(1) lookup.

**`PriceLevel`** — 64-byte cache-line aligned:
| Field       | Type       | Notes                             |
|-------------|------------|-----------------------------------|
| `head`      | `uint32_t` | index of first order at this level|
| `tail`      | `uint32_t` | index of last order               |
| `price`     | `int64_t`  | price for this level              |
| `total_qty` | `int64_t`  | aggregated quantity               |

`bids_[]` and `asks_[]` are flat arrays indexed by `price - min_price_`, so a price-level lookup is a single array offset — no hash map, no tree.

**Complexity summary:**

| Operation        | Complexity | Notes                                              |
|------------------|------------|----------------------------------------------------|
| insert           | O(1)       | free-list alloc + tail append                      |
| cancel           | O(1)       | `prev`/`next` unlink + free-list return            |
| modify (qty only)| O(1)       | in-place delta update                              |
| modify (price)   | O(1) unlink + O(range) best-price scan | scan only when removing last order at best price |
| getBestBid/Ask   | O(1) amortized | `best_bid_price_` / `best_ask_price_` cached   |

**`applyUpdate` invariant:** Cancel reads price from the node (not from the update message), so it bypasses the price-range guard. Add/Modify still validate `u.price` against `[min_price_, max_price_]`.

## APIs

- **SPSC ring:** `push`/`pop` non-blocking, power-of-two capacity — [`SpscRing`](src/core/ring_buffer.hpp).
- **FeedHandler:** consumer registration and `onUpdate` callback — [`FeedHandler`](src/feed/feed_handler.hpp).
- **OrderBook:** single entry point `applyUpdate(MarketUpdate)`; queries `[[nodiscard]] getBestBid` / `getBestAsk` — [`OrderBook`](src/core/order_book.hpp). Non-copyable, non-movable (owns raw arrays).
- **Strategy:** callbacks `on_market_update`, `on_timer`, optional `poll_signal` — [`src/engine/strategy_example.cpp`](src/engine/strategy_example.cpp).
- **EventLoop:** pulls from MD queue → `OrderBook::applyUpdate` → strategy → risk — [`EventLoop`](src/engine/event_loop.cpp).

## Replay and Zero-copy
Replay uses memory-mapped files to avoid copying; parser consumes bytes and returns consumed size (`parser.parse(ptr, end, u)`) — see [`src/replay/mmap_replay.cpp`](src/replay/mmap_replay.cpp) and [`src/feed/binary_parser.cpp`](src/feed/binary_parser.cpp).

## Performance Notes
- Hot path: `applyUpdate` → strategy callback → SPSC push; no heap allocations, no locks.
- `OrderNode` doubly-linked list (`prev`+`next`) enables O(1) cancel and O(1) price-change unlink. Best-price scan after emptying a level is bounded by the configured price range.
- `OrderNode` and `PriceLevel` are both `alignas(64)` to prevent false sharing and maximize cache utilization.
- Timers: monotonic ns via [`util/timer.hpp`](src/util/timer.hpp); event loop fires timers at `timer_interval_ns`.
- Use power-of-two queue sizes (e.g., `QUEUE_CAP = 1<<20`).

## Testing & Benchmarks
- Unit tests: [`tests/unit_order_book.cpp`](tests/unit_order_book.cpp) (19 cases: bid/ask insert, best-price tracking, qty/price modify, head/middle/tail cancel, node recycling, edge cases), [`tests/unit_ring_buffer.cpp`](tests/unit_ring_buffer.cpp).
- Integration test: [`tests/integration_event_loop.cpp`](tests/integration_event_loop.cpp).
- Order book microbench: [`benchmarks/bench_order_book.cpp`](benchmarks/bench_order_book.cpp) — TSC-calibrated, 1M samples, p50/p99/p99.9/max. See README for latest numbers.
- Feed throughput bench: [`benchmarks/feed_throughput.cpp`](benchmarks/feed_throughput.cpp) — exercises the full replay → feed handler → ring → order book pipeline.

## Build / Run
CMake + MinGW on Windows; build artifacts under `build/`.
```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
build/bench_order_book.exe
```

## Open Considerations
- `FixedPool` in [`src/util/memory_pool.hpp`](src/util/memory_pool.hpp) is currently unused (dead code). Candidate for removal or future use as a slab allocator for strategy objects.
- Resolve strategy output model: `DummyStrategy` demonstrates both `out_queue_.push` and `poll_signal`; pick one and remove the other.
- `RiskManager` interface: standardize method name (`check` vs `checkAndApply`).
- Port to Linux: swap `MapViewOfFile` → `mmap`, `SetThreadAffinityMask` → `sched_setaffinity`, add hugepage support (`MAP_HUGETLB`).
- Add a non-trivial strategy signal (e.g., EMA mid-price or order-book imbalance ratio).
