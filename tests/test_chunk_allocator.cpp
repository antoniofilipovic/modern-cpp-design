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

TEST(ChunkAllocatorTest, TestAllocateOneChunk){

}

TEST(MemoryPoolAllocatorTest, TestMemoryPoolReusesAllocations){

}

TEST(MemoryPoolAllocatorTest, TestMemoryIsAllocatable){

}