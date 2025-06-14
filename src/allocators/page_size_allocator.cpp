#include <cassert>
#include <cstdlib>
#include <iostream>

#include "page_size_allocator.hpp"

namespace memory{

std::size_t PageSizeAllocator::actualSizeAllocated(const std::size_t size) {
  if(size == 0) {
    return PAGE_SIZE;
  }
  auto new_size = (size/PAGE_SIZE) * PAGE_SIZE;
  if(size%PAGE_SIZE != 0) {
    new_size += PAGE_SIZE;
  }

  return new_size;
}

void *PageSizeAllocator::allocate(std::size_t alignment, std::size_t size){
  size = PageSizeAllocator::actualSizeAllocated(size);

  // if I allocate alignment + page_size -> 4096 + 4096 bytes -> this is always aligned on std::max_align_t which is
  // 16 bytes
  // in the metadata we can store whether we  have allocated it and also the number of pages we allocated

  // allocator will actually round allocation to the upper page size

  // TODO: use malloc to do page alignment yourself
  void* ptr = std::aligned_alloc(PAGE_SIZE, size);
  if(ptr != nullptr){
    pagesAllocated_ += size/PAGE_SIZE;
  }
  return ptr;
}

void PageSizeAllocator::deallocate(void *ptr){
  if(ptr!=nullptr){
    // this will probably needed some kind of metadata in the beginning of the page size
  }
  free(ptr);
  pagesFreed_ += 1;
}

PageSizeAllocator::~PageSizeAllocator()= default;

} // namespace memory