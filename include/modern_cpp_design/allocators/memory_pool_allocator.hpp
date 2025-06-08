#include <cstdint>
#include <page_size_allocator.hpp>
#include<vector>
#include <cassert>


namespace memory{
constexpr std::size_t BLOCK_SIZE = 512;
constexpr std::size_t NUM_BLOCKS_PER_PAGE = PAGE_SIZE / BLOCK_SIZE;
class MemoryPoolAllocator{


  public:
      // no locking in case of multi threaded applications
      explicit MemoryPoolAllocator(PageSizeAllocator &page_size_allocator): page_size_allocator_(page_size_allocator){}

      void *allocate(std::size_t alignment, std::size_t size){
          assert(size <= 512);

          if (free_pointers_.empty()) {
              void *ptr = page_size_allocator_.allocate(PAGE_SIZE, PAGE_SIZE);
              pages_ptrs_.emplace_back(ptr);
              assert(ptr != nullptr);
              pages_allocated_+=1;
              for(std::size_t i{0}; i<PAGE_SIZE/512; i++) {
                  free_pointers_.emplace_back(reinterpret_cast<void *>(reinterpret_cast<uint64_t>(ptr) + i * 512));
              }
          }
          assert(!free_pointers_.empty());


          // swap beginning and end
          std::iter_swap(free_pointers_.begin(), free_pointers_.end()-1);

          auto *allocated = free_pointers_.back();
          free_pointers_.pop_back();
          used_pointers_.emplace_back(allocated);
          return allocated;
      }

      void deallocate(void *ptr){

          auto const elem = std::find(used_pointers_.begin(), used_pointers_.end(), [&ptr](void* const curr) {
              return ptr == curr;
          });
          assert(elem != used_pointers_.end());
          // kick out the element
          std::iter_swap(elem, used_pointers_.end()-1);
          used_pointers_.pop_back();

          free_pointers_.emplace_back(ptr);
      }

      void release() {
          assert(used_pointers_.empty());
          for(void *ptr: pages_ptrs_) {
              page_size_allocator_.deallocate(ptr);
          }
          free_pointers_.clear();
      }

    ~MemoryPoolAllocator() {
          release();
      }



  private:

    std::vector<void*> free_pointers_;
    std::vector<void*> used_pointers_;
    std::vector<void*> pages_ptrs_;
    std::size_t pages_allocated_{0};
    PageSizeAllocator &page_size_allocator_;


};

}