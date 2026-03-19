#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <new>
#include <cassert>
#include <type_traits>
#include <cstring>
#include "market_data.hpp"

template<typename T>
class SpscRing {
public:
    explicit SpscRing(size_t capacity_pow2): capacity_(capacity_pow2), mask_(capacity_pow2 - 1), buffer_(nullptr) 
    {
        assert(capacity_pow2 != 0 && (capacity_pow2 & (capacity_pow2 - 1)) == 0 && "Capacity must be a power of 2");
        buffer_ = static_cast<T*>(::operator new[](sizeof(T) * capacity_, std::align_val_t(64)));
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }
    ~SpscRing() 
    {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_relaxed);
        while (tail != head) {
            buffer_[tail & mask_].~T();
            tail++;
        }
        ::operator delete[](buffer_, std::align_val_t(64));
    }

    bool push(const T& item)
    {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = head + 1;
        const size_t tail = tail_.load(std::memory_order_acquire);
        if(next_head - tail > capacity_) {
            return false; // Queue is full
        }
        std::memcpy(&buffer_[head & mask_], &item, sizeof(T));
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool push(T&& item) 
    {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = head + 1;
        const size_t tail = tail_.load(std::memory_order_acquire);
        if ((next - tail) > capacity_) return false; // full
        std::memcpy(&buffer_[head & mask_], &item, sizeof(T));
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out)
    {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);
        if(tail == head) {
            return false; // Queue is empty
        }
        out = std::move(buffer_[tail & mask_]);
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    void reset()
    {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_relaxed);
        while (tail != head) {
            buffer_[tail & mask_].~T();
            ++tail;
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    size_t size() const
    {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return head >= tail ? head - tail : 0;
    }

    bool empty() const { return size() == 0; }
private:
    const size_t capacity_;
    const size_t mask_;
    T* buffer_;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};