#include <gtest/gtest.h>
#include <string>
#include "page_size_allocator.hpp"


struct TestingStruct {
    explicit TestingStruct(std::string my_string, const double d, const int i): my_string_(std::move(my_string)), d_(d), i_(i) {}
    std::string my_string_;
    double d_;
    int i_;
};

TEST(PageAllocatorTest, BasicAssertions){
    memory::PageSizeAllocator page_size_allocator{};

    std::size_t allocated_size = memory::PAGE_SIZE;

    ASSERT_EQ(allocated_size, memory::PageSizeAllocator::actualSizeAllocated(1));

    void *p = page_size_allocator.allocate(1, 1);
    page_size_allocator.deallocate(p);

    ASSERT_EQ(page_size_allocator.getPagesAllocated(), 1);
    ASSERT_EQ(page_size_allocator.getPagesFreed(), 1);

    ASSERT_EQ(reinterpret_cast<uint64_t>(p) % 4096, 0); // should be aligned on page size
    std::cout << "my value: " << reinterpret_cast<uint64_t>(p) << std::endl;
    std::align(alignof(TestingStruct), sizeof(TestingStruct), p, allocated_size);
    ASSERT_EQ(allocated_size, 4096); // nothing was lost on alignment


    std::string const my_string{"some small string which does not have sbo"};
    std::construct_at<TestingStruct>(reinterpret_cast<TestingStruct*>(p), my_string, 2.0, 1);

    auto *my_struct = reinterpret_cast<TestingStruct*>(p);
    ASSERT_EQ(my_struct->my_string_, my_string);
    ASSERT_EQ(my_struct->d_, 2.0);
    ASSERT_EQ(my_struct->i_, 1);

}

TEST(PageAllocatorTest, TestSizeAllocated){
    memory::PageSizeAllocator page_size_allocator{};

    ASSERT_EQ(4096, memory::PageSizeAllocator::actualSizeAllocated(1));
    ASSERT_EQ(4096, memory::PageSizeAllocator::actualSizeAllocated(4096));
    ASSERT_EQ(4096, memory::PageSizeAllocator::actualSizeAllocated(0));
    ASSERT_EQ(4096*2, memory::PageSizeAllocator::actualSizeAllocated(4097));
}