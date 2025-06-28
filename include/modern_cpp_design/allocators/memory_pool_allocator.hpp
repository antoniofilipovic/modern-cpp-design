#pragma once

#include <vector>
#include <cassert>

#include "page_size_allocator.hpp"

namespace memory{
constexpr std::size_t BLOCK_SIZE = 512;
constexpr std::size_t NUM_BLOCKS_PER_PAGE = PAGE_SIZE / BLOCK_SIZE;

/**
 * Currently very simplified implementation of memory pool. In case it needs more memory
 * it calls page_size_allocator which provides page aligned memory
 * Once memory is in the memory pool it splits the page on blocks of a size of 512.
 * This would mean that free blocks will be on page_size + 0, page_size+512, ..., page_size+(4096-512)
 * Hence, the only alignment allowed is up to 512 bytes. If a block of a bigger alignment
 * is required, it would fail. (I should add a test for that).
 * Also, since allocator does not track to which page which block belongs to,
 * in case of all blocks belonging to one page are freed
 * it won't free the whole page, hence this one is
 */
class MemoryPoolAllocator{

  public:
      // no locking in case of multi threaded applications
      explicit MemoryPoolAllocator(PageSizeAllocator &page_size_allocator): page_size_allocator_(page_size_allocator){}

      void *allocate(std::size_t alignment, std::size_t size);

      void deallocate(void *ptr);

      void release();

      [[nodiscard]] std::size_t getNumFreeBlocks() const {
          return free_pointers_.size();
      }

      [[nodiscard]] std::size_t getNumUsedBlocks() const {
          return used_pointers_.size();
      }

      ~MemoryPoolAllocator() {
        release();
      }

  private:

    /**
     * List of free pointers of a size of 512
     */
    std::vector<void*> free_pointers_;

    /**
     * List of a used pointers.
     */
    std::vector<void*> used_pointers_;

    /**
      * List of pointers to the beginning of the page. We can't free
      * each block, but we can only return whole page
      */
    std::vector<void*> pages_ptrs_;

      /**
       * Number of pages allocated in total
       */
    std::size_t pages_allocated_{0};

    PageSizeAllocator &page_size_allocator_;


};

}