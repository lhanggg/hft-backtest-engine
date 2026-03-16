#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
        free_head_ = base_;
    }

    void* allocate()
    {
        // pop a node from the free list
        if (!free_head_) return nullptr;
        void* head = free_head_;
        free_head_ = *reinterpret_cast<void**>(head);
        return head;
    }

    void deallocate(void* p)
    {
        // push the node back to the free list
        *reinterpret_cast<void**>(p) = free_head_;
        free_head_ = p;
    }

    void release()
    {
        if(base_)
        {
            free(base_);
            base_ = nullptr;
            free_head_ = nullptr;
            count_ = 0;
        }
    }
private:
    size_t elemSize_;
    size_t count_{0};
    uint8_t* base_{nullptr};
    void* free_head_{nullptr};
};