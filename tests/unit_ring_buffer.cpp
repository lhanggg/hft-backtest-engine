#include "../src/core/ring_buffer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>


int main() {
  constexpr size_t CAP = 1024; // power of two
  SpscRing<uint64_t> ring(CAP);

  // basic single-thread push/pop
  for (uint64_t i = 0; i < 1000; ++i) {
    bool ok = ring.push(i);
    assert(ok);
  }
  for (uint64_t i = 0; i < 1000; ++i) {
    uint64_t v = 0;
    bool ok = ring.pop(v);
    assert(ok && v == i);
  }
  assert(ring.empty());

  // producer/consumer threads
  constexpr size_t N = 1000000;
  std::thread prod([&](){
    for (size_t i = 1; i <= N; ++i) {
      while (!ring.push(static_cast<uint64_t>(i))) {
        // busy-wait; in real bench add backoff
      }
    }
  });

  std::thread cons([&](){
    uint64_t expected = 1;
    uint64_t got = 0;
    while (expected <= N) {
      if (ring.pop(got)) {
        assert(got == expected);
        ++expected;
      }
    }
  });

  prod.join();
  cons.join();

  std::cout << "SPSC ring buffer unit test passed\n";

  
  return 0;
}