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


bool isChunkPointingToPrevAndNext(const Chunk &chunk, Chunk* prev, Chunk *next) {
    return chunk.getPrev() == prev && chunk.getNext() == next;
}

bool isChunkPrevAndNextSizeCorrect(const Chunk &chunk, std::size_t prev_size, std::size_t size, std::size_t flags) {
    return chunk.getSize() == size && chunk.getPrevSize() == prev_size && chunk.getFlags() == flags;
}

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
    // To test coalescing of the chunks, we need to allocate chunks which are not in the
    // fastbin range, as otherwise they will not be coalasced
    AfMalloc af_malloc{};


    void *ptr = af_malloc.malloc(FAST_BIN_RANGE_END + 10);
    Chunk *first_chunk = moveToThePreviousChunk(ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(*first_chunk, (Chunk{0, FAST_BIN_RANGE_END+32, nullptr, nullptr}));

    char *first_str = reinterpret_cast<char *>(ptr);
    // writing random stuff so we can test zeroing of memory
    strcpy(first_str, "lala");

    void *second_ptr = af_malloc.malloc(FAST_BIN_RANGE_END+25);
    Chunk *second_chunk = moveToThePreviousChunk(second_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(getMallocNeededSize(25), 48);
    ASSERT_EQ(*second_chunk, (Chunk{0, FAST_BIN_RANGE_END+48, nullptr, nullptr}));

    char *string_ptr = reinterpret_cast<char *>(second_ptr);
    strcpy(string_ptr, "po");


    void *third_ptr = af_malloc.malloc(FAST_BIN_RANGE_END+35);
    Chunk *third_chunk = moveToThePreviousChunk(third_ptr, HEAD_OF_CHUNK_SIZE);
    // 35 - 32 = 3 => 3 fits into the 8 bytes of the next chunk, add additional 16 bytes for HEAD_OF_CHUNK
    ASSERT_EQ(*third_chunk, (Chunk{0, FAST_BIN_RANGE_END+48, nullptr, nullptr}));


    char *third_str = reinterpret_cast<char *>(third_ptr);
    strcpy(third_str, "deda");


    af_malloc.free(ptr);
    auto *free_chunk_list = af_malloc.getUnsortedChunks();
    // Free chunk list is not pointing to self but to first chunk
    ASSERT_FALSE(isPointingToSelf(*free_chunk_list));

    // Assert the free chunk list start points correctly to the first chunk
    ASSERT_EQ(free_chunk_list->getNext(), first_chunk);
    ASSERT_EQ(free_chunk_list->getPrev(), first_chunk);

    // assert that first chunk points back to free_chunk_list
    // Also that prev_size is 0 and size is our size without isPrevFree flag set
    ASSERT_EQ(*first_chunk, (Chunk{0,  FAST_BIN_RANGE_END+32, free_chunk_list, free_chunk_list}));

    // Prev size of second chunk should be filled
    // We can't check next and prev as chunk is still using that space for user data
    ASSERT_TRUE(isChunkPrevAndNextSizeCorrect(*second_chunk, first_chunk->getSize(), (FAST_BIN_RANGE_END+48), PREV_FREE));

    // second free
    af_malloc.free(second_ptr);

    auto *free_chunk_list_second_free = af_malloc.getUnsortedChunks();
    ASSERT_EQ(free_chunk_list_second_free, free_chunk_list);
    ASSERT_TRUE(isChunkPointingToPrevAndNext(*free_chunk_list_second_free, first_chunk, first_chunk));

    ASSERT_EQ(first_chunk->getSize(), FAST_BIN_RANGE_END*2 + 32 + 48); // 32 + 48
    ASSERT_EQ(first_chunk->getPrevSize(), 0);


    char emptyBuffer[sizeof(Chunk)]{};
    // should be equal
    ASSERT_EQ(memcmp(&emptyBuffer, second_chunk, sizeof(Chunk)), 0);

    // What is written in the first chunk in size, that should be written in the third chunk also
    ASSERT_EQ(third_chunk->getPrevSize(),  FAST_BIN_RANGE_END*2 + 32 + 48);
    ASSERT_EQ(third_chunk->isPrevFree(), true);

    af_malloc.free(third_ptr);

    // After third chunk is freed we should have the following state in memory
    // [COALESCED FIRST and SECOND CHUNK][NEWLY FREED THIRD CHUNK][TOP.....]
    // At this point we coalesce first,second and third chunk and extend the top
    Chunk *third_free_chunk_list = af_malloc.getUnsortedChunks();
    ASSERT_TRUE(isPointingToSelf(*third_free_chunk_list));

    // third_chunk is original pointer to the memory
    // since I haven't called destroy on this object, it should still be alive, but otherwise I would need to use
    // bit_cast
    ASSERT_EQ(memcmp(&emptyBuffer, third_chunk, sizeof(Chunk)), 0);
    ASSERT_EQ(memcmp(&emptyBuffer, first_chunk, sizeof(Chunk)), 0);


    ASSERT_EQ(af_malloc.getTop(), first_chunk);
}


TEST_F(BasicAfMallocSizeAllocated, TestAfNoCoalescingFastChunks) {
    // We want to write the size in the prev size
    // how that should behave for top_chunk
    // when do we move free chunks to fast bins- > on malloc is the correct answer here
    // Test that fast chunks are not coalesce
    AfMalloc af_malloc{};


    void *ptr = af_malloc.malloc(10);
    Chunk *first_chunk = moveToThePreviousChunk(ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(*first_chunk, (Chunk{0, 32, nullptr, nullptr}));

    char *first_str = reinterpret_cast<char *>(ptr);
    strcpy(first_str, "lala");

    void *second_ptr = af_malloc.malloc(25);
    Chunk *second_chunk = moveToThePreviousChunk(second_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(*second_chunk, (Chunk{0, 48, nullptr, nullptr}));

    char *string_ptr = reinterpret_cast<char *>(second_ptr);
    strcpy(string_ptr, "po");


    void *third_ptr = af_malloc.malloc(35);
    Chunk *third_chunk = moveToThePreviousChunk(third_ptr, HEAD_OF_CHUNK_SIZE);
    // 35 - 32 = 3 => 3 fits into the 8 bytes of the next chunk, add additional 16 bytes for HEAD_OF_CHUNK
    ASSERT_EQ(*third_chunk, (Chunk{0, 48, nullptr, nullptr}));

    char *third_str = reinterpret_cast<char *>(third_ptr);
    strcpy(third_str, "deda");


    af_malloc.free(ptr);
    auto *free_chunk_list = af_malloc.getUnsortedChunks();
    // We should add fast chunk to unsorted chunks first, before
    // moving them to the fast chunk bin
    ASSERT_FALSE(isPointingToSelf(*free_chunk_list));


    // Assert the free chunk list start points correctly to the first chunk
    ASSERT_EQ(free_chunk_list->getNext(), first_chunk);
    ASSERT_EQ(free_chunk_list->getPrev(), first_chunk);

    // assert that first chunk points back to free_chunk_list
    // Also that prev_size is 0 and size is our size without isPrevFree flag set
    ASSERT_EQ(*first_chunk, (Chunk{0,  32, free_chunk_list, free_chunk_list}));

    // Prev size of second chunk should be filled
    // No flag should be set, (i.e IS_PREV_FREE)
    ASSERT_TRUE(isChunkPrevAndNextSizeCorrect(*second_chunk, first_chunk->getSize(), 48, EMPTY_FLAG));

    ASSERT_EQ(third_chunk->getPrevSize(), 0);

    // second free
    af_malloc.free(second_ptr);

    auto *free_chunk_list_second_free = af_malloc.getUnsortedChunks();
    ASSERT_FALSE(isPointingToSelf(*free_chunk_list_second_free));

    ASSERT_EQ(free_chunk_list_second_free, free_chunk_list);

    ASSERT_TRUE(isChunkPointingToPrevAndNext(*free_chunk_list_second_free, first_chunk, second_chunk));

    ASSERT_EQ(*first_chunk, (Chunk{0,  32, second_chunk, free_chunk_list_second_free}));
    ASSERT_EQ(*second_chunk, (Chunk{32,  48, free_chunk_list_second_free, first_chunk}));


    ASSERT_TRUE(isChunkPrevAndNextSizeCorrect(*third_chunk, second_chunk->getSize(), 48, EMPTY_FLAG));


    af_malloc.free(third_ptr);

    Chunk *third_free_chunk_list = af_malloc.getUnsortedChunks();
    ASSERT_FALSE(isPointingToSelf(*third_free_chunk_list));
    ASSERT_EQ(third_free_chunk_list, free_chunk_list);

    ASSERT_TRUE(isChunkPointingToPrevAndNext(*third_free_chunk_list, first_chunk, third_chunk));

    ASSERT_EQ(*first_chunk, (Chunk{0,  32, second_chunk, third_free_chunk_list}));
    ASSERT_EQ(*second_chunk, (Chunk{32,  48, third_chunk, first_chunk}));
    ASSERT_EQ(*third_chunk, (Chunk{48,  48, third_free_chunk_list, second_chunk}));

}


TEST_F(BasicAfMallocSizeAllocated, TestChunkIsMovedToTheCorrectBin) {
    // we need to trigger moving of the chunk by calling malloc after frees
    AfMalloc af_malloc{true};

    void *ptr_0 = af_malloc.malloc(10);
    void *ptr_1 = af_malloc.malloc(30);
    void *ptr_2 = af_malloc.malloc(100);
    void *ptr_3 = af_malloc.malloc(FAST_BIN_RANGE_END + 20);
    void *ptr_4 = af_malloc.malloc(105);
    void *ptr_5 = af_malloc.malloc(FAST_BIN_RANGE_END + 40);
    void *ptr_6 = af_malloc.malloc(25);
    af_malloc.free(ptr_0);
    af_malloc.free(ptr_1);
    af_malloc.free(ptr_2);
    af_malloc.free(ptr_3);
    af_malloc.free(ptr_4);
    af_malloc.free(ptr_5);
    af_malloc.free(ptr_6);
    af_malloc.dumpMemory();
    Chunk *unsorted_chunks = af_malloc.getUnsortedChunks();
    // This test case makes it impossible to coalesce chunks as they are allocated in between normal size chunks
    Chunk *chunk_6 = moveToThePreviousChunk(ptr_6, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(unsorted_chunks->getNext(), chunk_6);
    Chunk *chunk_5 = moveToThePreviousChunk(ptr_5, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(chunk_6->getNext(), chunk_5);
    Chunk *chunk_4 = moveToThePreviousChunk(ptr_4, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(chunk_5->getNext(), chunk_4);
    Chunk *chunk_3 = moveToThePreviousChunk(ptr_3, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(chunk_4->getNext(), chunk_3);
    Chunk *chunk_2 = moveToThePreviousChunk(ptr_2, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(chunk_3->getNext(), chunk_2);
    Chunk *chunk_1 = moveToThePreviousChunk(ptr_1, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(chunk_2->getNext(), chunk_1);
    Chunk *chunk_0 = moveToThePreviousChunk(ptr_0, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(chunk_1->getNext(), chunk_0);

    // this chunk does not exist in the unsorted chunks, and we should move our chunks to the new bins
    void *ptr_7 = af_malloc.malloc(FAST_BIN_RANGE_END + 200);

    af_malloc.dumpMemory();

    Chunk *unsorted_chunks_2 = af_malloc.getUnsortedChunks();
    ASSERT_TRUE(isPointingToSelf(*unsorted_chunks_2));

    auto &fast_bin_chunks = af_malloc.getFastBinChunks();

    // in the first bin there should be only chunk of 10 bytes
    {
        auto [bin, bit] = *findBinIndex(getMallocNeededSize(10));
        ASSERT_EQ(bin, FASTBINS_INDEX);
        ASSERT_EQ(bit, 2);
        ASSERT_EQ(getMallocNeededSize(10), 32);
        ASSERT_FALSE(isPointingToSelf(fast_bin_chunks[bit]));
        ASSERT_EQ(fast_bin_chunks[bit].getNext(), chunk_0);
        ASSERT_EQ(chunk_0->getNext(), &fast_bin_chunks[bit]);
    }
    //
    {
        auto [bin, bit] = *findBinIndex(getMallocNeededSize(25));
        // unsorted chunks should be chunk_7 ----> chunk_2
        // then we iterate chunk_7 --> chunk_2
        // then we have fast_bin_head ->> chunk_7, unsorted_chunks -> chunk_2
        // at the end: fast_bin_head -> chunk_2 ->chunk_7
        ASSERT_FALSE(isPointingToSelf(fast_bin_chunks[bit]));
        ASSERT_EQ(fast_bin_chunks[bit].getNext(), chunk_1);
        ASSERT_EQ(chunk_1->getNext(), chunk_6);
        ASSERT_EQ(chunk_6->getNext(), &fast_bin_chunks[bit]);
    }
    {
        auto [bin, bit] = *findBinIndex(getMallocNeededSize(100));
        ASSERT_EQ(bin, FASTBINS_INDEX);
        ASSERT_EQ(bit, 7);
        ASSERT_EQ(getMallocNeededSize(100), 112);
        ASSERT_FALSE(isPointingToSelf(fast_bin_chunks[bit]));
        ASSERT_EQ(fast_bin_chunks[bit].getNext(), chunk_2);
        ASSERT_EQ(chunk_2->getNext(), &fast_bin_chunks[bit]);

    }
    {
        auto [bin, bit] = *findBinIndex(getMallocNeededSize(105));
        ASSERT_EQ(bin, FASTBINS_INDEX);
        ASSERT_EQ(bit, 8);
        ASSERT_EQ(getMallocNeededSize(105), 128);
        ASSERT_FALSE(isPointingToSelf(fast_bin_chunks[bit]));
        ASSERT_EQ(fast_bin_chunks[bit].getNext(), chunk_4);
        ASSERT_EQ(chunk_4->getNext(), &fast_bin_chunks[bit]);

    }
    auto &small_bin_chunks = af_malloc.getSmallBinChunks();
    {
        auto [bin, bit] = *findBinIndex(getMallocNeededSize(180));
        ASSERT_EQ(bin, SMALLBINS_INDEX);
        ASSERT_EQ(bit, 2);
        ASSERT_EQ(getMallocNeededSize(180), 192);
        ASSERT_FALSE(isPointingToSelf(small_bin_chunks[bit]));
        ASSERT_EQ(small_bin_chunks[bit].getNext(), chunk_3);
        ASSERT_EQ(chunk_3->getNext(), &small_bin_chunks[bit]);
    }
    {
        auto [bin, bit] = *findBinIndex(getMallocNeededSize(200));
        ASSERT_EQ(bin, SMALLBINS_INDEX);
        ASSERT_EQ(bit, 3);
        ASSERT_EQ(getMallocNeededSize(200), 208);
        ASSERT_FALSE(isPointingToSelf(small_bin_chunks[bit]));
        ASSERT_EQ(small_bin_chunks[bit].getNext(), chunk_5);
        ASSERT_EQ(chunk_5->getNext(), &small_bin_chunks[bit]);
    }

    af_malloc.free(ptr_7);
}



TEST_F(BasicAfMallocSizeAllocated, TestAfMallocCoalasce3ChunksLIFO) {
    // allocate 1, 2, 3
    // free 3, 2, 1
    // 3 should be top of free chunks, then 2 then 1
    // we should properly coalasce them

    AfMalloc af_malloc{};

    void *ptr = af_malloc.malloc(FAST_BIN_RANGE_END+10);
    Chunk *first_chunk = moveToThePreviousChunk(ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(*first_chunk, (Chunk{0, FAST_BIN_RANGE_END+32, nullptr, nullptr}));

    char *first_str = static_cast<char *>(ptr);
    strcpy(first_str, "lala");

    void *second_ptr = af_malloc.malloc(FAST_BIN_RANGE_END+25);
    Chunk *second_chunk = moveToThePreviousChunk(second_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(*second_chunk, (Chunk{0, FAST_BIN_RANGE_END+48, nullptr, nullptr}));


    char *string_ptr = static_cast<char *>(second_ptr);
    strcpy(string_ptr, "po");


    void *third_ptr = af_malloc.malloc(FAST_BIN_RANGE_END+35);
    Chunk *third_chunk = moveToThePreviousChunk(third_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(*third_chunk, (Chunk{0, FAST_BIN_RANGE_END+48, nullptr, nullptr}));

    auto *third_str = static_cast<char *>(third_ptr);
    strcpy(third_str, "gagorago");

    char emptyBuffer[sizeof(Chunk)]{};
    // should be equal

    // Now we should see that we merge them again
    af_malloc.free(third_ptr);
    Chunk *free_chunk_list = af_malloc.getUnsortedChunks();
    ASSERT_TRUE(isPointingToSelf(*free_chunk_list));
    // Assert top has moved to the third chunk
    ASSERT_EQ(third_chunk, af_malloc.getTop());
    ASSERT_EQ(memcmp(&emptyBuffer, third_chunk, sizeof(Chunk)), 0);


    af_malloc.free(second_ptr);
    free_chunk_list = af_malloc.getUnsortedChunks();

    ASSERT_TRUE(isPointingToSelf(*free_chunk_list));
    ASSERT_EQ(second_chunk, af_malloc.getTop());
    ASSERT_EQ(memcmp(&emptyBuffer, second_chunk, sizeof(Chunk)), 0);



    af_malloc.free(ptr);

    free_chunk_list = af_malloc.getUnsortedChunks();
    ASSERT_TRUE(isPointingToSelf(*free_chunk_list));

    ASSERT_EQ(first_chunk, af_malloc.getTop());
    ASSERT_EQ(memcmp(&emptyBuffer, first_chunk, sizeof(Chunk)), 0);

}


TEST_F(BasicAfMallocSizeAllocated, TestReusingFreedChunks) {
    AfMalloc af_malloc{};

    void *ptr = af_malloc.malloc(10);
    Chunk *first_chunk = moveToThePreviousChunk(ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(first_chunk->getSize(), 32);
    void *second_ptr = af_malloc.malloc(10);
    Chunk *second_chunk = moveToThePreviousChunk(second_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(second_chunk->getSize(), 32);
    void *third_ptr = af_malloc.malloc(25);
    Chunk *third_chunk = moveToThePreviousChunk(third_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(third_chunk->getSize(), 48);

    af_malloc.free(second_ptr);
    void *fourth_ptr = af_malloc.malloc(10);

    Chunk *fourth_chunk = moveToThePreviousChunk(fourth_ptr, HEAD_OF_CHUNK_SIZE);
    ASSERT_EQ(second_chunk, fourth_chunk);
}

TEST_F(BasicAfMallocSizeAllocated, TestReusingRecentlyFreedChunk) {
    AfMalloc af_malloc{};

    auto *ptr_0 = af_malloc.malloc(25);
    Chunk *chunk_0 = moveToThePreviousChunk(ptr_0, HEAD_OF_CHUNK_SIZE);
    auto *ptr_1 = af_malloc.malloc(50);
    Chunk *chunk_1 = moveToThePreviousChunk(ptr_1, HEAD_OF_CHUNK_SIZE);

    auto *ptr_2 = af_malloc.malloc(45);
    Chunk *chunk_2 = moveToThePreviousChunk(ptr_2, HEAD_OF_CHUNK_SIZE);

    auto *ptr_3 = af_malloc.malloc(60);
    Chunk *chunk_3 = moveToThePreviousChunk(ptr_3, HEAD_OF_CHUNK_SIZE);

    af_malloc.free(ptr_2);
    af_malloc.free(ptr_0);

    ASSERT_EQ(chunk_0->getSize(), 48);
    // this means that we can't coalesce
    ASSERT_EQ(chunk_1->isPrevFree(), false);
    ASSERT_EQ(chunk_1->getPrevSize(), 48);

    ASSERT_EQ(chunk_2->getSize(), 64);
    // this means that we can't coalesce
    ASSERT_EQ(chunk_3->isPrevFree(), false);
    ASSERT_EQ(chunk_3->getPrevSize(), 64); // means we can coalesce later and it is free!

    ASSERT_EQ(af_malloc.getUnsortedChunks()->getNext(), chunk_0);

    void *ptr_4 = af_malloc.malloc(10);
    ASSERT_EQ(ptr_4, ptr_0);
    void *ptr_5 = af_malloc.malloc(44);
    ASSERT_EQ(ptr_5, ptr_2);

    ASSERT_TRUE(isPointingToSelf(*af_malloc.getUnsortedChunks()));

    ASSERT_EQ(chunk_0->getSize(), 48);
    ASSERT_EQ(chunk_1->isPrevFree(), false);
    ASSERT_EQ(chunk_1->getPrevSize(), 0);

    ASSERT_EQ(chunk_2->getSize(), 64);
    ASSERT_EQ(chunk_3->isPrevFree(), false);
    ASSERT_EQ(chunk_3->getPrevSize(), 0);

    af_malloc.free(ptr_0);
}


TEST_F(BasicAfMallocSizeAllocated, TestFastBinSmallBinChunkReusing) {
    AfMalloc af_malloc{true};

    auto *ptr_0 = af_malloc.malloc(25);
    Chunk *chunk_0 = moveToThePreviousChunk(ptr_0, HEAD_OF_CHUNK_SIZE);
    auto *ptr_1 = af_malloc.malloc(50);
    Chunk *chunk_1 = moveToThePreviousChunk(ptr_1, HEAD_OF_CHUNK_SIZE);

    auto *ptr_2 = af_malloc.malloc(FAST_BIN_RANGE_END+45);
    Chunk *chunk_2 = moveToThePreviousChunk(ptr_2, HEAD_OF_CHUNK_SIZE);

    auto *ptr_3 = af_malloc.malloc(60);
    Chunk *chunk_3 = moveToThePreviousChunk(ptr_3, HEAD_OF_CHUNK_SIZE);

    af_malloc.free(ptr_2);
    af_malloc.free(ptr_0);

    ASSERT_EQ(chunk_0->getSize(), 48);
    // this means that we can't coalesce
    ASSERT_EQ(chunk_1->isPrevFree(), false);
    ASSERT_EQ(chunk_1->getPrevSize(), 48);

    ASSERT_EQ(chunk_2->getSize(), 224);
    // this means that we can coalesce -> small bin chunk
    ASSERT_EQ(chunk_3->isPrevFree(), true);
    ASSERT_EQ(chunk_3->getPrevSize(), 224); // means we can coalesce later and it is free!

    ASSERT_EQ(af_malloc.getUnsortedChunks()->getNext(), chunk_0);

    // This malloc will trigger moving chunks from unsorted to specific lists
    void *ptr_4 = af_malloc.malloc(FAST_BIN_RANGE_END*2);
    af_malloc.dumpMemory();

    auto [small_bin, small_bit] = *findBinIndex(getMallocNeededSize(FAST_BIN_RANGE_END+45));
    ASSERT_EQ(small_bin, SMALLBINS_INDEX);
    ASSERT_EQ(small_bit, 4); // (45+8)/16 -> 3.3 -> 4
    ASSERT_TRUE(af_malloc.isBinBitIndexSet(small_bin, small_bit));
    // this will trigger reusing fast chunk
    void *ptr_5 = af_malloc.malloc(FAST_BIN_RANGE_END+45);
    // This will trigger reusing small bin chunk
    void *ptr_6 = af_malloc.malloc(25);
    af_malloc.dumpMemory();
    ASSERT_EQ(ptr_5, ptr_2);
    ASSERT_EQ(ptr_6, ptr_0);

    ASSERT_TRUE(isPointingToSelf(*af_malloc.getUnsortedChunks()));

    ASSERT_EQ(chunk_0->getSize(), 48);
    // these flags should be cleared
    ASSERT_EQ(chunk_1->isPrevFree(), false);
    ASSERT_EQ(chunk_1->getPrevSize(), 0);

    ASSERT_EQ(chunk_2->getSize(), 224);
    // these flags should be cleared now
    ASSERT_EQ(chunk_3->isPrevFree(), false);
    ASSERT_EQ(chunk_3->getPrevSize(), 0);

    af_malloc.free(ptr_0);
}

TEST_F(BasicAfMallocSizeAllocated, TestMoveFromFreeChunks) {

}


TEST_F(BasicAfMallocSizeAllocated, DoubleFree) {
    // This should be possible to detect?
}

// how to synchronize multiple threads allocating at the same time?
TEST_F(BasicAfMallocSizeAllocated, MultipleThreadsAllocating) {

}

// test for unaligned access
// create a simple struct which needs to be aligned on 128 bytes

TEST_F(BasicAfMallocSizeAllocated, MemAlignTestCase) {

}
