#include <page_size_allocator.hpp>
#include <cassert>
#include <cstdlib>
#include <iostream>

namespace memory{

void *PageSizeAllocator::allocate(std::size_t size, std::size_t alignment){
  assert(size % PAGE_SIZE == 0);
  assert(alignment % PAGE_SIZE == 0);

  // if I allocate alignment + page_size -> 4096 + 4096 bytes -> this is always aligned on std::max_align_t which is
  // 16 bytes
  // in the metadata we can store whether we  have allocated it and also the number of pages we allocated

  // TODO: use malloc to do page alignment yourself
  void* ptr = std::aligned_alloc(alignment, PAGE_SIZE);
  if(ptr != nullptr){
    pagesAllocated_ += size%PAGE_SIZE;
  }
  return ptr;
}

void PageSizeAllocator::deallocate(void *ptr){
  if(ptr!=nullptr){
    // this will probably needed some kind of metadata in the beginning of the page size
  }
  free(ptr);
}

} // namespace memory