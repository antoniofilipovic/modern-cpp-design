#include <gtest/gtest.h>
#include <string>

#include "afmalloc.hpp"

std::size_t get_alignment_size(void const* ptr, std::size_t alignment) {

    uintptr_t int_ptr{0};
    // memory pointer is pointer to address
    // pointer is variable which holds address of object
    // ptr = address of memory block
    // &ptr = pointer to address of memory block
    memcpy(&int_ptr, &ptr, sizeof(uintptr_t));
    // might be undefined, but we are not casting away const ness
    //const auto int_ptr = reinterpret_cast<uintptr_t>(const_cast<void *>(ptr));
    const auto aligned_ptr_int = (int_ptr - 1u + alignment) & -alignment;
    return  aligned_ptr_int - int_ptr;
}
static_assert(sizeof(uintptr_t) == sizeof(void *));
static_assert(sizeof(uintptr_t) == 8);

// std::size_t getAlignmentSize(void *ptr, std::size_t alignment) {
//     // to get alignment size you need to dived your current number with
//     // alignment, if modulo is not 0, add alignment - modulo to the new number and you will get alignment
// }

TEST(AfMallocTest, TestAfMallocGet3Chunks){
    const std::size_t total_allocated_size = 32 * 4096; // 32 pages == 128kB
    AFMalloc af_malloc{};

    void *ptr = af_malloc.malloc(10);
    char *first_str = reinterpret_cast<char *>(ptr);
    strcpy(first_str, "baba");
    const std::size_t first_ptr_usage_size = sizeof(Chunk) + 10; // 34
    ASSERT_EQ(total_allocated_size - first_ptr_usage_size, af_malloc.getFreeSize());

    const void *first_ptr_top = af_malloc.getTop();
    const void *first_ptr_begin = af_malloc.getBegin();
    ASSERT_EQ(first_ptr_usage_size, reinterpret_cast<uintptr_t>(first_ptr_top)-reinterpret_cast<uintptr_t>(first_ptr_begin));

    ////////////////////////////////////// second ptr allocation /////////////////

    void *second_ptr = af_malloc.malloc(25);
    char *second_str = reinterpret_cast<char *>(second_ptr);
    const auto *second_top = af_malloc.getTop();
    strcpy(second_str, "deda");
    // begin should not change
    ASSERT_EQ(first_ptr_begin, af_malloc.getBegin());
    const std::size_t alignment_size_for_second_chunk = get_alignment_size(first_ptr_top, alignof(Chunk));

    // TODO check: not sure if this is really 48 where we can store it next
    // should not we be able to store it on 8 byte boundary

    // 40 is a number divisible with 8 which is alignment of Chunk
    // chunk can be stored on 8 byte alignment boundary
    // sizeof Chunk is 24 bytes
    ASSERT_EQ(6, alignment_size_for_second_chunk);
    ASSERT_EQ(alignment_size_for_second_chunk, 40 - first_ptr_usage_size);

    // top was moved from the place where it was before to the new place. In between there should be allocation of
    // chunk, alignment for chunk and 25 bytes which user has requested
    // Checks that we have correctly updated the top
    //                                 25  + 24 + 6 = 55
    std::size_t second_ptr_used_size = 25 + sizeof(Chunk) + alignment_size_for_second_chunk;
    ASSERT_EQ(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(first_ptr_top) + second_ptr_used_size), second_top);
    ASSERT_EQ(total_allocated_size - second_ptr_used_size - first_ptr_usage_size, af_malloc.getFreeSize());
    ASSERT_EQ(reinterpret_cast<uintptr_t>(second_top) - reinterpret_cast<uintptr_t>(first_ptr_begin), 34 + 55);


    // total used size so far is (24+10) + (6 + 24 + 25) = 34 + 55 = 89
    // 89 bytes are currently used without allocating 35 bytes and alignment and chunk for the third chunk
    ///////////////////////////////////////// third ptr allocation /////////////////////////

    void *third_ptr = af_malloc.malloc(35);
    char *third_str = reinterpret_cast<char *>(third_ptr);
    strcpy(third_str, "deda");


    uintptr_t int_ptr{0};
    memcpy(&int_ptr, second_top, sizeof(uintptr_t));
    void *int_ptr_test = reinterpret_cast<void*>(int_ptr);
    std::size_t size = 100;
    std::cout << reinterpret_cast<uintptr_t>(second_top) << std::endl;
    std::align(alignof(Chunk), 1, int_ptr_test, size);
    std::cout << size << std::endl;
    const std::size_t alignment_size_for_third_chunk = get_alignment_size(second_top, alignof(Chunk));
    ASSERT_EQ(7, alignment_size_for_third_chunk);

    af_malloc.free(ptr);
    af_malloc.free(second_ptr);
    af_malloc.free(third_ptr);

}