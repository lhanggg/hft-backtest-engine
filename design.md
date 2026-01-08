Project Design for HFT Backtesting Engine
This document defines the architecture, data layouts, APIs, performance targets, and verification plan for the ultra‑low‑latency backtester. It is a practical blueprint you can implement end‑to‑end and use to produce reproducible benchmarks and resume‑grade results.


Goals and Performance Targets
- Primary goal: event‑driven C++ backtester with an order book capable of sub‑30 ns best‑price queries and sub‑microsecond end‑to‑end processing for 1M+ market updates/sec.
- Secondary goals: allocation‑free hot path, lock‑free data paths, NUMA‑aware threading, zero‑copy replay, deterministic replay, and reproducible profiling.
- Measured metrics: P50, P95, P99 latencies for update→query; throughput (msgs/sec); CPU utilization; tail latency under load.


High Level Architecture
- Feed Ingest → Ring Buffers → Order Book → Event Dispatcher → Strategy Engine → Risk/PnL → Execution Simulator / Logger.
- Storage and Replay: append‑only binary tick logs; mmap replay tool for deterministic input.
- Telemetry: lightweight histograms, rdtsc wrappers, perf/VTune traces, flamegraphs.

Module Designs and APIs
Ring Buffer Module
- Purpose: low‑latency handoff between producer and consumer threads.
- Types: SPSC (single producer single consumer), MPSC (multiple producers single consumer) if needed.
- API
template<typename T>
class SpscRing {
public:
  bool push(const T& item); // nonblocking, returns false if full
  bool pop(T& out);         // nonblocking, returns false if empty
  void reset();
};


- Implementation notes: use std::atomic<uint64_t> head/tail with acquire/release semantics; power‑of‑two capacity; mask index; avoid ABA by using sequence numbers if required; align head/tail to cache lines.
Memory Pool and Allocator
- Purpose: eliminate heap allocations on hot path.
- API
class FixedPool {
public:
  void* allocate();
  void deallocate(void* p);
  void preallocate(size_t n);
};


- Implementation notes: per‑thread pools, free lists stored in cache‑aligned nodes, use posix_memalign or aligned_alloc for 64‑byte alignment.
Order Book
- Purpose: maintain price levels and orders with O(1) best‑price queries and fast updates.
- Data layout
- Price level: fixed array or vector of buckets; each bucket contains head pointer to order list.
- Order node: fixed‑size struct with order id, price, qty, next index; stored in preallocated array.
- API
class OrderBook {
public:
  void applyUpdate(const MarketUpdate& u); // insert/modify/cancel
  bool getBestBid(PriceLevel& out);
  bool getBestAsk(PriceLevel& out);
  void snapshot(OrderBookSnapshot& s); // lockless snapshot if needed
};


- Implementation notes: use arrays indexed by price tick or hash; minimize branching in matching; keep hot fields cache aligned; avoid dynamic memory in applyUpdate.
Feed Handler and Binary Format
- Binary tick format (fixed width)
- uint64_t timestamp_ns; uint32_t msg_type; uint64_t order_id; int64_t price; int32_t qty; uint8_t side;
- Feed handler API
class FeedHandler {
public:
  void start(); // binds sockets, pins threads
  void stop();
  void registerConsumer(SpscRing<MarketUpdate>* ring);
};


- Implementation notes: use recvmmsg batching; use mmap for replay files; parse with branch‑free code paths; align buffers to cache lines.
Event Loop and Strategy Interface
- Event loop: poll ring buffers, dispatch events to strategy threads via SPSC queues.
- Strategy API
class Strategy {
public:
  void onMarketUpdate(const MarketUpdate& u);
  void onTimer(uint64_t now_ns);
};


- Implementation notes: pin strategy threads to cores; use per‑instrument affinity to reduce contention.
Risk and P&L
- Design: lock‑free counters for positions and P&L; atomic snapshots for checks.
- API
class RiskManager {
public:
  bool checkAndApply(const Order& o); // returns false if violates limits
  double getPnl();
};


- Implementation notes: pad per‑instrument counters to avoid false sharing; enforce limits on hot path with atomic compare‑and‑swap.

Data Layout Examples
struct alignas(64) OrderNode {
  uint64_t order_id;
  int64_t price;
  int32_t qty;
  uint32_t next_index;
  uint8_t side;
  uint8_t padding[7];
};
struct MarketUpdate {
  uint64_t ts_ns;
  uint32_t type;
  uint64_t order_id;
  int64_t price;
  int32_t qty;
  uint8_t side;
};


- Guideline: align hot structs to 64 bytes; keep hot fields at the beginning; avoid pointers in hot arrays—use indices.

Concurrency and NUMA Strategy
- Thread model
- 1 thread per NIC RX queue or replay reader (feed threads).
- 1 thread per group of instruments (strategy threads) pinned to cores.
- 1 thread for risk aggregation and logging.
- NUMA
- allocate memory on the same NUMA node as the thread that will access it (numa_alloc_onnode or mbind).
- pin threads using sched_setaffinity.
- Affinity policy
- map instruments to threads deterministically; avoid cross‑socket sharing of hot data.

Zero Copy and Kernel Bypass Plan
- Replay: use mmap to map tick files and feed them into ring buffers without copying.
- Live ingest: use recvmmsg with large buffers and batching; optionally prototype DPDK or VMA as a documented extension.
- Syscall minimization: batch system calls and avoid per‑message syscalls on hot path.

Profiling and Measurement
- Timing primitives: rdtsc wrappers with calibration to ns; clock_gettime for wall time.
- Tools: perf, Intel VTune, perf record/report, flamegraphs, perf stat for hardware counters.
- Metrics to capture: P50/P95/P99 latencies, throughput, CPU cycles per message, branch misses, cache misses, context switches.
- Benchmark harness: microbench for order book single‑threaded; integration bench for feed→orderbook→strategy.

Testing and Validation
- Unit tests: order book invariants, ring buffer correctness, memory pool behavior.
- Stress tests: ASAN/TSAN builds for concurrency bugs; long‑running replay at target rates.
- Deterministic replay: seed RNGs and use fixed tick files to reproduce results.
- Regression CI: run microbenchmarks on CI hardware or a controlled runner; fail on latency regressions beyond thresholds.

Reproducibility and Environment Notes
- Hardware: list CPU model, core count, frequency, NIC model, RAM, NUMA topology.
- Kernel: record kernel version and tuning (IRQ affinity, net.core.rmem_max, net.core.rmem_default, vm.swappiness, hugepages).
- Build: use CMake with -O3 -march=native -flto for release; provide debug and perf builds.
- Runbook: scripts to set CPU affinity, disable turbo boost if needed, set IRQ affinity, and run replay at target rates.

Microbench Plan and Acceptance Criteria
- Microbench 1 Order Book Single Thread
- Goal: median query < 30 ns; P99 < 200 ns.
- Method: prepopulate book, run 10M random queries, measure latencies.
- Microbench 2 Feed Throughput
- Goal: sustain 1M updates/sec end‑to‑end with sub‑microsecond processing.
- Method: replay synthetic feed at target rate, measure throughput and tail latency.
- Microbench 3 Concurrency
- Goal: scale to 100+ instruments with <2x latency increase.
- Method: run multi‑threaded replay with pinned threads and measure per‑instrument latency.
- Acceptance: publish P50/P95/P99 numbers with hardware and kernel config; include perf flamegraphs showing hotspots.

