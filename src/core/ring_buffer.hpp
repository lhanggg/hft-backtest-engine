#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <new>
#include <cassert>
#include <type_traits>
#include "market_data.hpp"

template<typename T>
class SpscRing {
public:
    explicit SpscRing(size_t capacity_pow2): _capacity(capacity_pow2), _mask(capacity_pow2 - 1), _buffer(nullptr) 
    {
        assert(capacity_pow2 != 0 && (capacity_pow2 & (capacity_pow2 - 1)) == 0 && "Capacity must be a power of 2");
        _buffer = static_cast<T*>(::operator new[](sizeof(T) * _capacity, std::align_val_t(64)));
        _head.store(0, std::memory_order_relaxed);
        _tail.store(0, std::memory_order_relaxed);
    }
    ~SpscRing() 
    {
        size_t head = _head.load(std::memory_order_relaxed);
        size_t tail = _tail.load(std::memory_order_relaxed);
        while (tail != head) {
            _buffer[tail & _mask].~T();
            tail++;
        }
        ::operator delete[](_buffer, std::align_val_t(64));
    }

    bool push(const T& item)
    {
        const size_t head = _head.load(std::memory_order_relaxed);
        const size_t next_head = head + 1;
        const size_t tail = _tail.load(std::memory_order_acquire);
        if(next_head - tail > _capacity) {
            return false; // Queue is full
        }
        new (&_buffer[head & _mask]) T(item);
        _head.store(next_head, std::memory_order_release);
        return true;
    }

    bool push(T&& item) 
    {
        const size_t head = _head.load(std::memory_order_relaxed);
        const size_t next = head + 1;
        const size_t tail = _tail.load(std::memory_order_acquire);
        if ((next - tail) > _capacity) return false; // full
        new (&_buffer[head & _mask]) T(std::move(item));
        _head.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out)
    {
        const size_t tail = _tail.load(std::memory_order_relaxed);
        const size_t head = _head.load(std::memory_order_acquire);
        if(tail == head) {
            return false; // Queue is empty
        }
        out = std::move(_buffer[tail & _mask]);
        _tail.store(tail + 1, std::memory_order_release);
        return true;
    }

    // destruct remaing items and reset head and tail
    void reset()
    {
        size_t head = _head.load(std::memory_order_relaxed);
        size_t tail = _tail.load(std::memory_order_relaxed);
        while (tail != head) {
            _buffer[tail & _mask].~T();
            ++tail;
        }
        _head.store(0, std::memory_order_relaxed);
        _tail.store(0, std::memory_order_relaxed);
    }

    size_t size() const
    {
        const size_t head = _head.load(std::memory_order_acquire);
        const size_t tail = _tail.load(std::memory_order_acquire);
        return head >= tail ? head - tail : 0;
    }

    bool empty() const { return size() == 0; }
private:
    const size_t _capacity;
    const size_t _mask;
    T* _buffer;
    alignas(64) std::atomic<size_t> _head;
    alignas(64) std::atomic<size_t> _tail;
};