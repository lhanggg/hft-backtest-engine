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
    FixedPool(size_t elemSize = 64): _elemSize(std::max(elemSize, sizeof(void*))){}
    ~FixedPool(){ release();}
    void preallocate(size_t n)
    {
        release();
        _count = n;
        // allocate contiguous block aligned to 64 bytes
        size_t blockSize = _elemSize * n;
        int rc = posix_memalign(reinterpret_cast<void**>(&_base), 64, blockSize);
        if(rc != 0) {
            _base = nullptr;
        }
        // build free list using pointers stored inside nodes
        uint8_t* p = _base;
        for(size_t i = 0; i < n; ++i){
            void** slot = reinterpret_cast<void**>(p + i * _elemSize);
            void* next = (i + 1 < n) ? (p + (i + 1) * _elemSize) : nullptr;
            *slot = next;
        }
        _free_head.store(_base, std::memory_order_relaxed);
    }

    void* allocate()
    {
        // pop a node from the free list
        void* head = _free_head.load(std::memory_order_relaxed);
        if(!head) return nullptr;
        void* next = *(reinterpret_cast<void**>(head));
        _free_head.store(next, std::memory_order_relaxed);
        return head;
    }

    void deallocate(void* p)
    {
        // push the node back to the free list
        void* old = _free_head.load(std::memory_order_relaxed);
        *(reinterpret_cast<void**>(p)) = old;
        _free_head.store(p, std::memory_order_relaxed);
    }

    void release()
    {
        if(_base)
        {
            _base = nullptr;
            _free_head.store(nullptr, std::memory_order_relaxed);
            _count = 0;
        }
    }
private:
    size_t _elemSize;
    size_t _count{0};
    uint8_t* _base{nullptr};
    std::atomic<void*> _free_head{nullptr};
};