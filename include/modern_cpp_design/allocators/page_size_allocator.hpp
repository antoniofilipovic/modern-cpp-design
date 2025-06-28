//
// Created by antonio-filipovic on 3/29/25.
//

// check what does this exactly mean
#pragma once

#include <array>
// implement from Modern CPP book

namespace memory{

constexpr std::size_t PAGE_SIZE = 4096;
constexpr std::size_t NUM_PAGES = 4096/sizeof(void*);

/**
* This allocator will return page sized objects which later you can reuse and set up a pool
* which can reuse that memory and report it back when it is freed
* Currently there are limitations in terms of how to find when the block is freed.
* That should be solved if we have linked list of pages, and use start and end of the page as a bookkeeping
* mechanism to free the pages
*/

/**
 * Page size allocator can return multiple of the page size. If requested size is smaller than the page size,
* it rounds it up to the page size. In case some pages are freed, it consolidates pages one next to the other into
* the one contiguous block of memory. 
*/
class PageSizeAllocator{

  public:

    explicit PageSizeAllocator() = default;

    static std::size_t actualSizeAllocated(std::size_t);

    // for now let's say that size can only be of 4096 bytes,
    void* allocate(std::size_t alignment, std::size_t size);

    // this is a problem because we don't know where is this pointer stored in our array
	void deallocate(void *ptr);

    [[nodiscard]] std::size_t getPagesAllocated() const {
        return pagesAllocated_;
    }

    [[nodiscard]] std::size_t getPagesFreed() const {
        return pagesFreed_;
    }

    ~PageSizeAllocator();

  private:
      std::size_t pagesAllocated_{0};
      std::size_t pagesFreed_{0};
};





} // namespace memory