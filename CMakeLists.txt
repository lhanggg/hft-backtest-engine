cmake_minimum_required(VERSION 3.20)
project(trading_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------------------------------------------------
# Include directories
# ----------------------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

# ----------------------------------------------------------------------
# trading_core library
# ----------------------------------------------------------------------
add_library(trading_core
    # core
    src/core/order_book.cpp
    src/core/order_book.hpp
    src/core/market_data.hpp
    src/core/ring_buffer.hpp

    # feed
    src/feed/binary_parser.cpp
    src/feed/binary_parser.hpp
    src/feed/feed_handler.cpp
    src/feed/feed_handler.hpp

    # replay
    src/replay/mmap_replay.cpp
    src/replay/mmap_replay.hpp

    # engine
    src/engine/event_loop.cpp
    src/engine/event_loop.hpp
    src/engine/strategy_interface.hpp
    src/engine/strategy_example.cpp

    # util (header-only)
    src/util/memory_pool.hpp
    src/util/timer.hpp
    src/util/cpu_affinity.hpp

    # risk (header-only for now)
    src/risk/risk_manager.hpp
)

target_include_directories(trading_core PUBLIC src)

# ----------------------------------------------------------------------
# Benchmarks
# ----------------------------------------------------------------------
add_executable(bench_order_book
    benchmarks/bench_order_book.cpp
)
target_link_libraries(bench_order_book PRIVATE trading_core)

add_executable(feed_throughput
    benchmarks/feed_throughput.cpp
)
target_link_libraries(feed_throughput PRIVATE trading_core)

# ----------------------------------------------------------------------
# tools (optional executables)
# ----------------------------------------------------------------------
add_executable(generate_feed
    src/tools/generate_feed.cpp
)
target_link_libraries(generate_feed PRIVATE trading_core)

# ----------------------------------------------------------------------
# tests
# ----------------------------------------------------------------------
enable_testing()

add_executable(unit_order_book tests/unit_order_book.cpp)
target_link_libraries(unit_order_book PRIVATE trading_core)
add_test(NAME unit_order_book COMMAND unit_order_book)

add_executable(unit_ring_buffer tests/unit_ring_buffer.cpp)
target_link_libraries(unit_ring_buffer PRIVATE trading_core)
add_test(NAME unit_ring_buffer COMMAND unit_ring_buffer)

add_executable(integration_event_loop tests/integration_event_loop.cpp)
target_link_libraries(integration_event_loop PRIVATE trading_core)
add_test(NAME integration_event_loop COMMAND integration_event_loop)
