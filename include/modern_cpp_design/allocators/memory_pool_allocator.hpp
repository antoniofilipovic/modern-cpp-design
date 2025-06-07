#include <cstdint>
#include <page_size_allocator.hpp>
#include<vector>
#include <cassert>


namespace memory{
constexpr std::size_t BLOCK_SIZE = 512;
constexpr std::size_t NUM_BLOCKS_PER_PAGE = PAGE_SIZE / BLOCK_SIZE;
class MemoryPoolAllocator{


  public:
      explicit MemoryPoolAllocator(PageSizeAllocator &page_size_allocator): page_size_allocator_(page_size_allocator){

      }

      void *allocate(std::size_t alignment, std::size_t size){
          assert(size <= 512);
          void *ptr = page_size_allocator_.allocate(PAGE_SIZE, PAGE_SIZE);

          assert(ptr != nullptr);

          for(std::size_t i{0}; i<PAGE_SIZE/512; i++) {
              // is uint8_t good value?
              // reinterpret cast should work here
              free_pointers_.emplace_back(reinterpret_cast<void *>(reinterpret_cast<uint64_t>(ptr) + i * 512));
          }

          // swap beginning and and
          std::swap(free_pointers_.begin(), free_pointers_.end()-1);
          auto *allocated = free_pointers_.back();
          free_pointers_.pop_back();
          return allocated;
      }

      void deallocate(void *ptr){
          // p
        free_pointers_.emplace_back(ptr);
      }
      void release() {
          /// todo think about fragmentation
      }

  private:

    std::vector<void*> free_pointers_;

    std::vector<void*> used_pointers_;
    PageSizeAllocator &page_size_allocator_;


};

}