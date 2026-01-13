# Project Design — Backtesting Engine (derived from code)

This document summarizes architecture, data layouts, APIs, and operational notes inferred from the codebase.

## Goals
- Ultra-low-latency event-driven backtester: target 1M+ updates/sec with sub-microsecond processing on the hot path.
- Allocation-free hot paths, lock-free handoff, deterministic replay, and reproducible profiling.

## High-level Architecture
Feed ingest → Feed handler → SPSC ring buffers → Order book → Event loop → Strategy → Risk → Execution/logging.

Key components in the repo:
- Producer / parser: [`BinaryParser::parse`](src/feed/binary_parser.cpp) and generator [`src/tools/generate_feed.cpp`](src/tools/generate_feed.cpp).
- Replay/mmap ingestion: [`run_mmap_replay`](src/replay/mmap_replay.cpp) maps feed files and feeds the handler.
- Feed handler: [`FeedHandler`](src/feed/feed_handler.hpp).
- Ring buffers: [`SpscRing`](src/core/ring_buffer.hpp).
- Market model: [`MarketUpdate`](src/core/market_data.hpp) and [`OrderBook`](src/core/order_book.hpp).
- Engine: [`EventLoop`](src/engine/event_loop.cpp) and [`Strategy`/`DummyStrategy`](src/engine/strategy_example.cpp).
- Risk: [`RiskManager`](src/risk/) (interface used by the engine).

## Data Layouts and Formats
- Feed record: 32‑byte packed record in [`src/tools/generate_feed.cpp`](src/tools/generate_feed.cpp); replay uses mmap in [`src/replay/mmap_replay.cpp`](src/replay/mmap_replay.cpp).
- In-memory event: [`MarketUpdate`](src/core/market_data.hpp) — small PODs passed via SPSC ring.

## APIs
- SPSC ring: push/pop non-blocking (power-of-two capacity) — see [`SpscRing`](src/core/ring_buffer.hpp).
- FeedHandler: consumer registration and onUpdate callback — [`FeedHandler`](src/feed/feed_handler.hpp).
- Strategy: callbacks `on_market_update`, `on_timer`, and optional `poll_signal` — see [`src/engine/strategy_example.cpp`](src/engine/strategy_example.cpp).
- Event loop: pulls from MD queue, applies updates to [`OrderBook`](src/core/order_book.hpp), invokes strategy, and enforces risk checks — see [`EventLoop`](src/engine/event_loop.cpp).

## Replay and Zero-copy
- Replay uses memory-mapped files to avoid copying; parser consumes bytes and returns consumed size (`parser.parse(ptr, end, u)`) — see [`src/replay/mmap_replay.cpp`](src/replay/mmap_replay.cpp) and [`src/feed/binary_parser.cpp`](src/feed/binary_parser.cpp).

## Performance Notes
- Hot path: applyUpdate → strategy callback → SPSC push; avoid locks and heap allocations.
- Timers: monotonic ns via `util/timer.hpp`; event loop fires timers at configured interval (`timer_interval_ns`) — see [`src/engine/event_loop.cpp`](src/engine/event_loop.cpp).
- Use power-of-two queue sizes (e.g., QUEUE_CAP = 1<<20 used in benchmarks).

## Testing & Benchmarks
- Unit tests: [`tests/unit_order_book.cpp`](tests/unit_order_book.cpp), [`tests/unit_ring_buffer.cpp`](tests/unit_ring_buffer.cpp).
- Benchmarks: [`benchmarks/feed_throughput.cpp`](benchmarks/feed_throughput.cpp) uses replay → feed handler → ring to exercise pipeline.

## Build / Run
- CMake + MinGW on Windows; build artifacts and compile commands under `build/` (see `build/compile_commands.json`).
- Typical run: generate feed via `src/tools/generate_feed.cpp`, then run benchmark/replay executable to feed the pipeline.

## Operational & Validation
- Pin threads, align buffers to cache lines, and measure via RDTS/CLOCK wrappers in [`util/timer.hpp`].
- Regression: run replay with fixed seed files and validate order book invariants with unit tests.

## Open considerations
- Formalize RiskManager interface and consistent naming (`check` vs `checkAndApply` in design notes).
- Decide single model for strategy output: push into ring vs poll_signal; `DummyStrategy` shows both models (`out_queue_.push` vs `poll_signal`).

For locations referenced above, see:
- [`src/core/ring_buffer.hpp`](src/core/ring_buffer.hpp) — SPSC ring
- [`src/core/market_data.hpp`](src/core/market_data.hpp) — MarketUpdate
- [`src/core/order_book.hpp`](src/core/order_book.hpp) — OrderBook
- [`src/feed/binary_parser.cpp`](src/feed/binary_parser.cpp) — BinaryParser::parse
- [`src/feed/feed_handler.hpp`](src/feed/feed_handler.hpp) — FeedHandler
- [`src/replay/mmap_replay.cpp`](src/replay/mmap_replay.cpp) — run_mmap_replay
- [`src/engine/event_loop.cpp`](src/engine/event_loop.cpp) — EventLoop
- [`src/engine/strategy_example.cpp`](src/engine/strategy_example.cpp) — DummyStrategy example
- [`src/tools/generate_feed.cpp`](src/tools/generate_feed.cpp) — feed generator