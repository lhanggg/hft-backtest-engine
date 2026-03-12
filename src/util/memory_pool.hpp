#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <cassert>
#include <atomic>
#include <memory>
#include <algorithm>

class FixedPool{
public:
    FixedPool(size_t elemSize = 64): elemSize_(std::max(elemSize, sizeof(void*))){}
    ~FixedPool(){ release();}
    void preallocate(size_t n)
    {
        release();
        count_ = n;
        // allocate contiguous block aligned to 64 bytes
        size_t blockSize = elemSize_ * n;
        int rc = posix_memalign(reinterpret_cast<void**>(&base_), 64, blockSize);
        if(rc != 0) {
            base_ = nullptr;
        }
        // build free list using pointers stored inside nodes
        uint8_t* p = base_;
        for(size_t i = 0; i < n; ++i){
            void** slot = reinterpret_cast<void**>(p + i * elemSize_);
            void* next = (i + 1 < n) ? (p + (i + 1) * elemSize_) : nullptr;
            *slot = next;
        }
        free_head_.store(base_, std::memory_order_relaxed);
    }

    void* allocate()
    {
        // pop a node from the free list
        void* head = free_head_.load(std::memory_order_relaxed);
        if(!head) return nullptr;
        void* next = *(reinterpret_cast<void**>(head));
        free_head_.store(next, std::memory_order_relaxed);
        return head;
    }

    void deallocate(void* p)
    {
        // push the node back to the free list
        void* old = free_head_.load(std::memory_order_relaxed);
        *(reinterpret_cast<void**>(p)) = old;
        free_head_.store(p, std::memory_order_relaxed);
    }

    void release()
    {
        if(base_)
        {
            free(base_);
            base_ = nullptr;
            free_head_.store(nullptr, std::memory_order_relaxed);
            count_ = 0;
        }
    }
private:
    size_t elemSize_;
    size_t count_{0};
    uint8_t* base_{nullptr};
    std::atomic<void*> free_head_{nullptr};
};