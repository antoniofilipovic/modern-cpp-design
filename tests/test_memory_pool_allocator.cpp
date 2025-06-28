#include <gtest/gtest.h>
#include <string>

#include "memory_pool_allocator.hpp"
#include "page_size_allocator.hpp"


struct TestingStruct {
    explicit TestingStruct(std::string my_string, const double d, const int i): my_string_(std::move(my_string)), d_(d), i_(i) {}
    std::string my_string_;
    double d_;
    int i_;
};

TEST(MemoryPoolAllocatorTest, TestMemoryPoolAllocatesOnlyOnePage){
    // TODO should I just set using namespace memory
    // using namespace memory;
    memory::PageSizeAllocator page_size_allocator{};

    memory::MemoryPoolAllocator memory_pool_allocator{page_size_allocator};

    std::vector<void *> ptrs;
    constexpr std::size_t num_blocks = memory::PAGE_SIZE / memory::BLOCK_SIZE;
    ptrs.reserve(num_blocks);

    void *ptr = memory_pool_allocator.allocate(1, 1);
    ASSERT_NE(ptr, nullptr);
    ptrs.emplace_back(ptr);
    ASSERT_EQ(page_size_allocator.getPagesAllocated(), 1);

    // since memory pool allocator splits blocks on 512 byte chunks


    for(std::size_t i{0}; i < num_blocks - 1; ++i) {
        void *new_ptr = memory_pool_allocator.allocate(1, 1);
        ASSERT_NE(new_ptr, nullptr);
        ASSERT_EQ(page_size_allocator.getPagesAllocated(), 1);
        ptrs.emplace_back(new_ptr);
    }

    std::sort(ptrs.begin(), ptrs.end(), [](auto const &ptr1, auto const &ptr2) -> bool {
        return reinterpret_cast<uint64_t>(ptr1) < reinterpret_cast<uint64_t>(ptr2);
    });

    for(std::size_t i{0}; i < num_blocks-1; ++i) {
        auto const &current_ptr = ptrs[i];
        auto const &next_ptr = ptrs[i+1];

        auto current_ptr_address = reinterpret_cast<uint64_t>(current_ptr);
        auto next_ptr_address = reinterpret_cast<uint64_t>(next_ptr);
        // We verify that they are back to back
        ASSERT_EQ(next_ptr_address - current_ptr_address,512);
    }


    std::for_each(ptrs.begin(), ptrs.end(), [&memory_pool_allocator](auto const &ptr) {
        memory_pool_allocator.deallocate(ptr);
    });

    ASSERT_EQ(0, memory_pool_allocator.getNumUsedBlocks());
    ASSERT_EQ(8, memory_pool_allocator.getNumFreeBlocks());
}

TEST(MemoryPoolAllocatorTest, TestMemoryPoolReusesAllocations){

}

TEST(MemoryPoolAllocatorTest, TestMemoryIsAllocatable){

}