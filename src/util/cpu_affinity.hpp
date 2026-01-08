#pragma once
#include <windows.h>
#include <cstdint>

// Pin the current thread to a specific CPU core on Windows.
// core_id = 0, 1, 2, ...
inline void pin_thread_to_core(std::uint32_t core_id) {
    DWORD_PTR mask = (1ull << core_id);
    HANDLE thread = GetCurrentThread();
    SetThreadAffinityMask(thread, mask);
}