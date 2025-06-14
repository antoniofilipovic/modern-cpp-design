#include "memory_pool_allocator.hpp"
#include <cstdint>
#include <algorithm>
namespace memory {
    void *MemoryPoolAllocator::allocate(std::size_t alignment, std::size_t size){
        assert(size <= 512);
        assert(alignment <= 512);

        if (free_pointers_.empty()) {
            void *ptr = page_size_allocator_.allocate(alignment, size);
            pages_ptrs_.emplace_back(ptr);
            assert(ptr != nullptr);
            pages_allocated_+=1;
            for(std::size_t i{0}; i<PAGE_SIZE/BLOCK_SIZE; i++) {
                free_pointers_.emplace_back(reinterpret_cast<void *>(reinterpret_cast<uint64_t>(ptr) + i * 512));
            }
        }
        assert(!free_pointers_.empty());

        // swap beginning and end
        std::iter_swap(free_pointers_.begin(), free_pointers_.end()-1);

        auto *free_ptr = free_pointers_.back();
        free_pointers_.pop_back();
        used_pointers_.emplace_back(free_ptr);
        return free_ptr;
    }

    void MemoryPoolAllocator::deallocate(void *ptr) {
        auto const elem = std::find_if(used_pointers_.begin(), used_pointers_.end(), [&ptr](void * const &curr) {
            return reinterpret_cast<uint64_t>(ptr) == reinterpret_cast<uint64_t>(curr);
        });
        assert(elem != used_pointers_.end());
        // kick out the element
        std::iter_swap(elem, used_pointers_.end()-1);
        used_pointers_.pop_back();

        free_pointers_.emplace_back(ptr);
    }

    void MemoryPoolAllocator::release() {
        assert(used_pointers_.empty());
        for(void *ptr: pages_ptrs_) {
            page_size_allocator_.deallocate(ptr);
        }
        free_pointers_.clear();
    }

} //namespace memory
