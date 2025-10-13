#include <gtest/gtest.h>
#include <string>

#include "AfMalloc.hpp"

/**
 * This function returns additional bytes needed to align on the `alignment` bytes
 * @param ptr for which to check alignment
 * @param alignment size of alignment
 * @return number of bytes needed to align on alignment
 */
std::size_t getAlignmentSizeTest(void const* ptr, std::size_t alignment) {
    const auto ptr_value = reinterpret_cast<uintptr_t>(const_cast<void *>(ptr));
    // This is a general trick to find alignment.
    // If you want to align on 16 bytes, you add 15 and then remove the modulo.
    // This means if pointer is aligned on 16 bytes it would not do anything, otherwise it will
    // find that alignment
    const auto aligned_ptr_value = (ptr_value + (alignment - 1u)) & -alignment;
    return  aligned_ptr_value - ptr_value;
}
static_assert(sizeof(uintptr_t) == sizeof(void *));
static_assert(sizeof(uintptr_t) == 8);


class BasicAfMallocSizeAllocated : public ::testing::Test {
public:
    BasicAfMallocSizeAllocated() = default;
};

TEST_F(BasicAfMallocSizeAllocated, AllocatedSize) {
    // for everything up to 24 bytes, we need to return 32 bytes which will be allocated
    // The reason is that we can use space in the Chunk for the forward and backward pointer and
    // from the next chunk we can use 8 bytes
    ASSERT_EQ(32, getMallocNeededSize(23));
    ASSERT_EQ(32, getMallocNeededSize(8));
    ASSERT_EQ(32, getMallocNeededSize(1));
    ASSERT_EQ(32, getMallocNeededSize(0));


}

TEST_F(BasicAfMallocSizeAllocated, TestAfMallocGet3Chunks){
    // This test checks allocation for 3 chunks where each allocation would fit inside one chunk and would not
    // spill over to the next neighboring chunk

    const std::size_t total_allocated_size = 32 * 4096; // 32 pages == 128kB
    AfMalloc af_malloc{};

    void *ptr = af_malloc.malloc(10);
    char *first_str = reinterpret_cast<char *>(ptr);
    strcpy(first_str, "baba");
    const std::size_t first_ptr_usage_size = 32;
    ASSERT_EQ(first_ptr_usage_size, getMallocNeededSize(10));
    ASSERT_EQ(total_allocated_size - af_malloc.getFreeSize(), first_ptr_usage_size);

    const void *first_ptr_top = af_malloc.getTop();
    const void *first_ptr_begin = af_malloc.getBegin();
    // we expect the top to be aligned on 16 byte boundary
    ASSERT_EQ(getAlignmentSizeTest(first_ptr_top, 16), 0);
    ASSERT_EQ(getAlignmentSizeTest(ptr, 16), 0);

    ////////////////////////////////////// second ptr allocation /////////////////

    void *second_ptr = af_malloc.malloc(25);
    char *string_ptr = reinterpret_cast<char *>(second_ptr);

    strcpy(string_ptr, "deda");
    // begin should not change
    ASSERT_EQ(first_ptr_begin, af_malloc.getBegin());

    std::size_t second_ptr_used_size = getMallocNeededSize(25);
    ASSERT_EQ(second_ptr_used_size, 48); // in the one Chunk we can fit only 24 bytes, so we need additional 16 bytes
    // actually in one physical chunk we can fit 16 bytes, and we borrow 8 from the next one

    // assert that block of memory we get from af malloc is aligned on 16 bytes
    ASSERT_EQ(getAlignmentSizeTest(second_ptr, ALIGNMENT), 0);

    const void *second_top = af_malloc.getTop();
    ASSERT_EQ(getAlignmentSizeTest(second_top, ALIGNMENT), 0); // we still moved our top to 16 byte boundary

    ///////////////////////////////////////// third ptr allocation /////////////////////////

    void *third_ptr = af_malloc.malloc(35);
    char *third_str = reinterpret_cast<char *>(third_ptr);
    strcpy(third_str, "deda");

    ASSERT_EQ(0, getAlignmentSizeTest(second_top, ALIGNMENT));



    af_malloc.free(ptr);
    af_malloc.free(second_ptr);
    af_malloc.free(third_ptr);

}

TEST_F(BasicAfMallocSizeAllocated, TestAfMallocCoalasce3Chunks) {
    const std::size_t total_allocated_size = 32 * 4096; // 32 pages == 128kB
    AfMalloc af_malloc{};

    void *ptr = af_malloc.malloc(10);
    Chunk *first_chunk = moveToThePreviousChunk(ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(first_chunk->getSize(), 32);
    ASSERT_EQ(first_chunk->getPrevSize(), 0);
    ASSERT_EQ(first_chunk->getNext(), nullptr);
    ASSERT_EQ(first_chunk->getPrev(), nullptr);

    char *first_str = reinterpret_cast<char *>(ptr);
    strcpy(first_str, "lala");

    void *second_ptr = af_malloc.malloc(25);
    Chunk *second_chunk = moveToThePreviousChunk(second_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(second_chunk->getSize(), 48);
    ASSERT_EQ(second_chunk->getPrevSize(), 0);
    ASSERT_EQ(second_chunk->getNext(), nullptr);
    ASSERT_EQ(second_chunk->getPrev(), nullptr);

    char *string_ptr = reinterpret_cast<char *>(second_ptr);
    strcpy(string_ptr, "po");


    void *third_ptr = af_malloc.malloc(35);
    Chunk *third_chunk = moveToThePreviousChunk(third_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(third_chunk->getSize(), 48); // 35 - 32 = 3 => 3 fits into the 8 bytes of the next chunk, add additional 16 bytes for HEAD_OF_CHUNK
    ASSERT_EQ(third_chunk->getPrevSize(), 0);
    ASSERT_EQ(third_chunk->getNext(), nullptr);
    ASSERT_EQ(third_chunk->getPrev(), nullptr);
    char *third_str = reinterpret_cast<char *>(third_ptr);
    strcpy(third_str, "deda");


    af_malloc.free(ptr);
    auto *free_chunks = af_malloc.getFreeChunks();
    // does pointer equality
    ASSERT_EQ(free_chunks, first_chunk);
    ASSERT_EQ(first_chunk->getNext(), first_chunk);
    ASSERT_EQ(first_chunk->getPrev(), first_chunk);
    ASSERT_EQ(first_chunk->getPrevSize(), 0);
    ASSERT_EQ(first_chunk->getSize(), 32);
    ASSERT_EQ(first_chunk->isPrevFree(), false);


    ASSERT_EQ(second_chunk->getPrevSize(), 32);
    ASSERT_EQ(second_chunk->getSize(), 48);
    ASSERT_EQ(second_chunk->isPrevFree(), true);

    ASSERT_EQ(third_chunk->getPrevSize(), 0);

    // second free
    af_malloc.free(second_ptr);

    auto *free_chunks_ptr2 = af_malloc.getFreeChunks();
    ASSERT_EQ(free_chunks_ptr2, first_chunk);
    ASSERT_EQ(first_chunk->getSize(), 80); // 32 + 48
    ASSERT_EQ(first_chunk->getPrevSize(), 0);
    ASSERT_EQ(first_chunk->getNext(), first_chunk);
    ASSERT_EQ(first_chunk->getPrev(), first_chunk);


    // should assert equal to 0 memory
    ASSERT_EQ(second_chunk->getSize(), 0);
    ASSERT_EQ(second_chunk->getNext(), nullptr);
    ASSERT_EQ(second_chunk->isPrevFree(), false);
    ASSERT_EQ(second_chunk->getPrevSize(), 0);

    ASSERT_EQ(third_chunk->getPrevSize(), 80);
    ASSERT_EQ(third_chunk->isPrevFree(), true);

    af_malloc.free(third_ptr);

    Chunk *third_free_chunk = af_malloc.getFreeChunks();
    ASSERT_EQ(third_free_chunk, nullptr);
    ASSERT_EQ(third_chunk->getSize(), 0);
    ASSERT_EQ(third_chunk->isPrevFree(), false);
    ASSERT_EQ(third_chunk->getNext(), nullptr);
    ASSERT_EQ(third_chunk->getPrev(), nullptr);

    ASSERT_EQ(af_malloc.getTop(), first_chunk);

}

TEST_F(BasicAfMallocSizeAllocated, TestAfMallocCoalasce3ChunksLIFO) {
    // allocate 1, 2, 3
    // free 3, 2, 1
    // 3 should be top of free chunks, then 2 then 1
    // we should properly coalasce them

    AfMalloc af_malloc{};

    void *ptr = af_malloc.malloc(10);
    Chunk *first_chunk = moveToThePreviousChunk(ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(first_chunk->getSize(), 32);
    ASSERT_EQ(first_chunk->getPrevSize(), 0);
    ASSERT_EQ(first_chunk->getNext(), nullptr);
    ASSERT_EQ(first_chunk->getPrev(), nullptr);

    char *first_str = static_cast<char *>(ptr);
    strcpy(first_str, "lala");

    void *second_ptr = af_malloc.malloc(25);
    Chunk *second_chunk = moveToThePreviousChunk(second_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(second_chunk->getSize(), 48);
    ASSERT_EQ(second_chunk->getPrevSize(), 0);
    ASSERT_EQ(second_chunk->getNext(), nullptr);
    ASSERT_EQ(second_chunk->getPrev(), nullptr);

    char *string_ptr = static_cast<char *>(second_ptr);
    strcpy(string_ptr, "po");


    void *third_ptr = af_malloc.malloc(35);
    Chunk *third_chunk = moveToThePreviousChunk(third_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(third_chunk->getSize(), 48);
    ASSERT_EQ(third_chunk->getPrevSize(), 0);
    ASSERT_EQ(third_chunk->getNext(), nullptr);
    ASSERT_EQ(third_chunk->getPrev(), nullptr);
    auto *third_str = static_cast<char *>(third_ptr);
    strcpy(third_str, "gagorago");



    // Now we should see that we merge them again
    af_malloc.free(third_ptr);
    Chunk *free_chunk_linked_list = af_malloc.getFreeChunks();
    ASSERT_EQ(free_chunk_linked_list, nullptr);
    ASSERT_EQ(third_chunk, af_malloc.getTop());
    ASSERT_EQ(third_chunk->getSize(), 0);
    ASSERT_EQ(third_chunk->getPrevSize(), 0);
    ASSERT_EQ(third_chunk->getNext(), nullptr);
    ASSERT_EQ(third_chunk->getPrev(), nullptr);


    af_malloc.free(second_ptr);
    free_chunk_linked_list = af_malloc.getFreeChunks();

    ASSERT_EQ(nullptr, free_chunk_linked_list);
    ASSERT_EQ(second_chunk, af_malloc.getTop());
    ASSERT_EQ(second_chunk->getSize(), 0);
    ASSERT_EQ(second_chunk->getPrevSize(), 0);
    ASSERT_EQ(second_chunk->getNext(), nullptr);
    ASSERT_EQ(second_chunk->getPrev(), nullptr);


    af_malloc.free(ptr);

    free_chunk_linked_list = af_malloc.getFreeChunks();
    ASSERT_EQ(free_chunk_linked_list, nullptr);
    ASSERT_EQ(first_chunk, af_malloc.getTop());
    ASSERT_EQ(first_chunk->getSize(), 0);
    ASSERT_EQ(first_chunk->getPrevSize(), 0);
    ASSERT_EQ(first_chunk->getNext(), nullptr);
    ASSERT_EQ(first_chunk->getPrev(), nullptr);
}

// test for unaligned access
// create a simple struct which needs to be aligned on 128 bytes



/// Try to allocate size of 24 bytes and write there
/// The thing is we should move the top before
