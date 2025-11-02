#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <memory>
#include <cstring>
#include <optional>
#include <algorithm>

#include "AfMalloc.hpp"

#include <sys/mman.h>

#define MMAP(addr, size, prot, flags) \
 mmap(addr, (size), (prot), (flags)|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)


static_assert(sizeof(long long int) == 8);


Chunk *moveToThePreviousChunk(void *ptr, std::size_t size) {
    return reinterpret_cast<Chunk *>(reinterpret_cast<uintptr_t>(ptr) - size);
}

Chunk *moveToTheNextChunk(void *ptr, std::size_t size) {
    return reinterpret_cast<Chunk *>(reinterpret_cast<uintptr_t>(ptr) + size);
}

void* moveToTheNextPlaceInMem(void *ptr, std::size_t size) {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + size);
}

void* moveToThePreviousPlaceInMem(void *ptr, std::size_t size) {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) - size);
}

std::size_t get_alignment_size(void* ptr, std::size_t alignment) {
    const auto int_ptr = reinterpret_cast<uintptr_t>(ptr);
    const auto aligned_ptr_int = (int_ptr + (alignment - 1u)) & -alignment;
    return  aligned_ptr_int - int_ptr;
    /// -alignment what is does is it creates a number which is  000010000  ->  111011111 + 1 -> 11100000  111111010000
    /// if ptr is aligned on something other than 16, when you add 15 it will move it to the next alignment slot
}


std::size_t get_needed_size_with_alignment(void *ptr, std::size_t alignment, std::size_t size) {
    const auto int_ptr = reinterpret_cast<uintptr_t>(ptr);
    const auto aligned_needed_ptr_int = (int_ptr +  size + (alignment - 1u)) & -alignment;
    return  aligned_needed_ptr_int - int_ptr;
}
std::size_t getMallocNeededSize(std::size_t size) {
    if (size + SIZE_OF_SIZE <= CHUNK_SIZE) {
        return CHUNK_SIZE;
    }
    // The reason behind this formula is that we need to satisfy user's request for `size` bytes
    // and we need 8 bytes to store the size (look at the chunk and check that it requires to store user's size)
    // Other fields are not used when chunk is allocated. So when our chunk is allocated the next chunk does not
    // use prev_size field and we don't use the fields for m_prev and m_next
    //

    // Anatomy of the chunk in this case
    // ----|+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    // ----| a) Size of previous chunk   |
    // allocated chunk starts here-->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    // ----| b) Current size            |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    //     | c) Ptr to next chunk              |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    //     | d1) Ptr to previous chunk          |
    //     | d2) rest of the user data |
    // next_chunk starts -> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    ///   | e) Previous size |
    ///   all again
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    // Now, to satisfy user request of the `size` bytes we have following space at use:
    //      c) + d) + e) (e from the next chunk)
    // c) and d1) are used as user space when chunk is allocated
    // e) which is memory from the next chunk is also used as a free space for our chunk
    // in other words: c) + d) + e) must be bigger or equal to the `size`
    // ( (c + d + e) + b) is actually one chunk, but which starts at the current_size , instead of prev_size_
    // ((size) +  (b)) -> size + SIZE_OF_SIZE
    // In that formula we add also ALIGNMENT_MASK

    return (size + SIZE_OF_SIZE + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;
}


std::optional<std::pair<std::size_t, std::size_t>> findBinIndex(const std::size_t allocations_size) {
    // 1. All our allocations are divisible with size of 16
    // 2. Our bins are made so that spacing between those bins is 16 bytes
    //
    assert(allocations_size % ALIGNMENT == 0);

    if(allocations_size < FAST_BIN_RANGE_END) {
        return std::make_pair(FASTBINS_INDEX, allocations_size / BIN_SPACING_SIZE);
    }if(allocations_size < SMALL_BIN_RANGE_END) {
        return std::make_pair(SMALLBINS_INDEX, (allocations_size - FAST_BIN_RANGE_END) / BIN_SPACING_SIZE);
    }
    return std::nullopt;
}


void clearUpDataSpaceOfChunk(Chunk *chunk) {
    auto *data_start =  moveToTheNextPlaceInMem(chunk, HEAD_OF_CHUNK_SIZE); // move to the place where data starts
    memset(data_start, 0, chunk->getSize() - HEAD_OF_CHUNK_SIZE);
}

bool isChunkCoalescable(const Chunk &chunk) {
    return chunk.getSize() > FAST_BIN_RANGE_END;
}


AfArena::AfArena() : bin_indexes_(2) {
    // TODO eat own dog food for bin indexes?
}


void AfMalloc::moveToUnsortedLargeChunks(Chunk *free_chunk) {
    Chunk *next_chunk = af_arena_.unsorted_large_chunks_.getNext();

    free_chunk->setNext(next_chunk);
    next_chunk->setPrev(free_chunk);


    free_chunk->setPrev(&af_arena_.unsorted_large_chunks_);
    af_arena_.unsorted_large_chunks_.setNext(free_chunk);

}

void AfMalloc::moveToFastBinsChunks(Chunk *free_chunk, std::size_t bit_index) {

}

void AfMalloc::moveToSmallBinsChunks(Chunk *free_chunk, std::size_t bit_index) {

}

void AfMalloc::extendTopChunk(){
    auto *top_chunk = static_cast<Chunk *>(getTop());
    assert(top_chunk->isPrevFree());
    Chunk *prev_chunk = moveToThePreviousChunk(top_chunk, top_chunk->getPrevSize());
    assert(prev_chunk->getNext() == nullptr && prev_chunk->getPrev() == nullptr);
    clearUpDataSpaceOfChunk(prev_chunk);
    af_arena_.top_ = prev_chunk;
    prev_chunk->unsetPrevFree();
    prev_chunk->setSize(0);
}

void unlinkChunk(Chunk* chunk) {
    // The only precondition here is that
    // next and prev chunk are not pointing to itself

    auto *nextChunk = chunk->getNext();
    assert(nextChunk != chunk);
    auto *prevChunk = chunk->getPrev();
    assert(prevChunk != chunk);

    nextChunk->setPrev(prevChunk);
    prevChunk->setNext(nextChunk);
}

AfMalloc::AfMalloc() {
    init();
}

void AfMalloc::init() {
    // TODO this should be implemented that we allocate memory with our own allocator and size
    af_arena_.fast_chunks_.resize(NUM_FAST_CHUNKS, {0, 0, nullptr, nullptr});
    std::ranges::for_each(af_arena_.fast_chunks_, [](Chunk &chunk) {
        chunk.setNext(&chunk);
        chunk.setPrev(&chunk);
    });

    af_arena_.small_chunks_.resize(NUM_FAST_CHUNKS, {0, 0, nullptr, nullptr});
    std::ranges::for_each(af_arena_.small_chunks_, [](auto &chunk) {
        chunk.setNext(&chunk);
        chunk.setPrev(&chunk);
    });
    // Set chunks to point to itself
    af_arena_.unsorted_large_chunks_ = {0, 0, nullptr, nullptr};
    af_arena_.unsorted_large_chunks_.setNext(&af_arena_.unsorted_large_chunks_);
    af_arena_.unsorted_large_chunks_.setPrev(&af_arena_.unsorted_large_chunks_);

    af_arena_.unsorted_chunks_ = {0, 0, nullptr, nullptr};
    af_arena_.unsorted_chunks_.setNext(&af_arena_.unsorted_chunks_);
    af_arena_.unsorted_chunks_.setPrev(&af_arena_.unsorted_chunks_);


    // only FAST and SMALL bin indexes live here
    af_arena_.bin_indexes_.resize(2, {0});

}


// Once deallocation is done, we write to the next chunk that prev_size is our size current, and we write to
// ourselves that we are not in use

// That should enable merging of two chunks
void AfMalloc::free(void *p) {
    // TODO detect double free -> that should be easy?
    /**
     * If chunk next to the top chunk is free, then we extend top chunk. That is why we never have
     * inside the top chunk the prev_size or isPrevFree set inside the size although there is enough space for that
    */
    auto *free_chunk = moveToThePreviousChunk(p, HEAD_OF_CHUNK_SIZE);

    clearUpDataSpaceOfChunk(free_chunk);

    // Here we want to check if the chunk in the physical memory before us has actually
    // been freed. If so, we can try to merge those two
    if(free_chunk->isPrevFree() && isChunkCoalescable(*free_chunk)) {
        // previous chunk is free, we need to merge them
        std::size_t prev_size = free_chunk->getPrevSize();

        /// Then we need to go to that place in the memory
        auto *chunk_before = moveToThePreviousChunk(free_chunk, prev_size);
        // if we don't do this here, then we will later have a problem
        // with merging two chunks and iterating over free chunks because of zeroing of memory

        unlinkChunk(chunk_before);
        //std::destroy_at<Chunk>(chunk_before);

        chunk_before->setSize( prev_size + free_chunk->getSize());
        free_chunk = chunk_before;
        clearUpDataSpaceOfChunk(free_chunk);
    }

    auto *next_chunk = moveToTheNextChunk(free_chunk, free_chunk->getSize());
    // Our chunk is not free until now. It can't be as we are only merging it now.
    assert(!next_chunk->isPrevFree());

    // This works even if we are at the at top, as on the top chunk we don't write size
    Chunk *chunk_two_hops_in_front =  moveToTheNextChunk(next_chunk, next_chunk->getSize());

    // if the next_chunk is free, we will merge the `freeChunk` and the `nextChunk`
    // otherwise `nextChunk` is allocated, and we can't merge these two
    if(chunk_two_hops_in_front->isPrevFree() && isChunkCoalescable(*free_chunk)) {
        // nextChunk is free so we need to merge that one too
        free_chunk->setSize(free_chunk->getSize() + next_chunk->getSize());

        unlinkChunk(next_chunk);
        // TODO add destroy at
        //std::destroy_at<Chunk>(next_chunk);

        clearUpDataSpaceOfChunk(free_chunk);
        chunk_two_hops_in_front->setPrevFree();
        chunk_two_hops_in_front->setPrevSize(free_chunk->getSize());
    }
    // when the nextChunk is free, if chunkTwoHopsInFront was free, it would be merged in the step before.
    // This way we know that our chunk is free, and only one step around us can be free

    // We need to find where is the next chunk, as we might have merged it in the step before
    next_chunk = moveToTheNextChunk(free_chunk, free_chunk->getSize());

    if(next_chunk < af_arena_.top_) {
        // Set that our chunk is free, only if it is not fast chunk.
        // By not setting it for the fast chunk, we disable coalasceing for the fast chunks
        if(isChunkCoalescable(*free_chunk)) {
            // Set on the next that chunk before is free
            next_chunk->setPrevFree();
            next_chunk->setPrevSize(free_chunk->getSize());
        }else {
            // this is needed for force coalescing of fast chunks later
            next_chunk->setPrevSize(free_chunk->getSize());
        }
    }else {
        if(isChunkCoalescable(*free_chunk)) {
            // next chunk here is arena.top_
            assert(next_chunk == af_arena_.top_);
            // in this part of code we extend top to the free_chunk
            // this means we can't add free chunk to the free list
            static_cast<Chunk*>(af_arena_.top_)->setPrevFree();
            static_cast<Chunk*>(af_arena_.top_)->setPrevSize(free_chunk->getSize());
            // here we should actually merge our chunk with the top, and that way we have extended the unlimited free chunk
            extendTopChunk();
            // We have extended the top, the rest of the code deals with adding the chunk to the unsorted chunks
            return;
        }else {
            static_cast<Chunk*>(af_arena_.top_)->setPrevSize(free_chunk->getSize());
        }
    }

    // We append to the top of the list newly freed chunk
    Chunk *head_chunk = &af_arena_.unsorted_chunks_;
    if(isPointingToSelf(*head_chunk)) {
        head_chunk->setNext(free_chunk);
        head_chunk->setPrev(free_chunk);

        free_chunk->setNext(head_chunk);
        free_chunk->setPrev(head_chunk);
        return;
    }

    Chunk *last_in_chunk = head_chunk->getNext();

    free_chunk->setPrev(head_chunk);
    free_chunk->setNext(last_in_chunk);

    last_in_chunk->setPrev(free_chunk);
    head_chunk->setNext(free_chunk);
}

AfMalloc::~AfMalloc() {
    if(af_arena_.free_size_ != af_arena_.allocated_size_) {
        std::cout << "leaking memory" << std::endl;
    }
    munmap(af_arena_.begin_, af_arena_.allocated_size_);
}

void AfMalloc::moveChunkToCorrectBin(Chunk *current_chunk, std::size_t needed_size) {
    auto maybe_bin_index = findBinIndex(needed_size);
    // free_chunk_list -> 1 -> 2 - > 3
    if(!maybe_bin_index) {
        moveToUnsortedLargeChunks(current_chunk);
    }else {
        auto [index, bit_index] = *maybe_bin_index;
        if(index == FASTBINS_INDEX) {
            // fast range
            moveToFastBinsChunks(current_chunk, bit_index);
        }else {
            // small range
            moveToSmallBinsChunks(current_chunk, bit_index);
        }
    }

}

std::optional<void*> AfMalloc::findChunkFromUnsortedFreeChunks(std::size_t needed_size) {
    // Next to the free chunk, unless it is in the fast bin range, there will always be an allocated chunk,
    // since otherwise we would coalesce them
    // on the free.
    // For the fast bin chunk, even if the chunk next to the fast bin chunk is free, we would not coalesce them.

    // Unsorted free chunks are stored in a double linked list
    Chunk *start  = &af_arena_.unsorted_chunks_;
    assert(start->getNext() != nullptr);
    Chunk *current_chunk = start->getNext();
    Chunk *match{nullptr};
    while(current_chunk != start) {
        // We are looking for the first chunk that we can find.
        // If we encounter a chunk which is not of needed size, we will move it to the appropriate bin
        if(current_chunk->getSize()  >= needed_size) {
            match = current_chunk;
            break;
        }
        unlinkChunk(current_chunk);
        moveChunkToCorrectBin(current_chunk, needed_size);
        current_chunk = current_chunk->getNext();
    }

    if(match == nullptr) {
        return std::nullopt;
    }

    unlinkChunk(match);

    // TODO this is opportunity to split the chunk on the multiple chunks, since we could otherwise get really
    // big chunk
    Chunk* next_chunk = moveToTheNextChunk(match, match->getSize());
    // next chunk only knows that we are free
    next_chunk->unsetPrevFree();
    // this part of memory will be used by our chunk also, hence we need to zero the memory
    next_chunk->setPrevSize(0x0000);

    // We don't need to call the construct here since we have already done so
    return moveToTheNextPlaceInMem(match, HEAD_OF_CHUNK_SIZE);
}

std::size_t getMaxFastBinBitIndex() {
    return FAST_BIN_RANGE_END / BIN_SPACING_SIZE;
}

std::size_t getMaxSmallBinBitIndex() {
    return (SMALL_BIN_RANGE_END - FAST_BIN_RANGE_END) / BIN_SPACING_SIZE;
}

bool isInFastBinRange(std::size_t size) {
    return size <= FAST_BIN_RANGE_END;
}

bool isInSmallBinRange(std::size_t size) {
    return size >= FAST_BIN_RANGE_END && size <= SMALL_BIN_RANGE_END;
}

bool hasLargeChunkFree(Chunk *large_chunk) {
    return large_chunk != nullptr;
}


void AfMalloc::setBinIndex(std::size_t bin, std::size_t bit) {
    af_arena_.bin_indexes_[bin] |= (1ul << bit);
}

void AfMalloc::unsetBitIndex(std::size_t bin, std::size_t bit) {
    af_arena_.bin_indexes_[bin] &= ~(1ul << bit);
}

bool AfMalloc::isBinBitIndexSet(std::size_t bin, std::size_t bit) {
    return af_arena_.bin_indexes_[bin].test(bit);
}

/**
 * Fast bin chunk gets allocated, we remove it from the list
 * Later we add it to the unsorted chunks, and insert back into the free list
 * It is never coalesced however.
 * @param size
 * @return
 */
Chunk *AfMalloc::tryFindFastBinChunk(const std::size_t size) {
    auto [fast_bin_index, bit_index] = *findBinIndex(size);
    assert(fast_bin_index == FASTBINS_INDEX);
    auto index = bit_index;

    // Traverse only up to 2 blocks away from our chunk
    while(index < getMaxFastBinBitIndex() && (index - bit_index <= 2)) {
        Chunk &chunk_list = af_arena_.fast_chunks_[index];
        if(isPointingToSelf(chunk_list)) {
            unsetBitIndex(fast_bin_index, bit_index);
        }else {
            // Not sure how malloc does this, but probably good idea to restrict this to one above
            // if there is no exact match, otherwise we are wasting a lot of memory space
            Chunk *next_chunk = chunk_list.getNext();
            unlinkChunk(next_chunk);
            assert(next_chunk != nullptr);
            return next_chunk;
        }
        index++;
    }
    return nullptr;
}

/**
 * Try to find chunk which is of same size, or one size larger.
 * Not sure if we should iterate more here, and then just split the chunk if found
 * @param size
 * @return
 */
Chunk *AfMalloc::tryFindSmallBinChunk(std::size_t size) {
    std::vector<Chunk > &small_chunks = af_arena_.small_chunks_;
    auto [small_bin_index, bit_index] = *findBinIndex(size);
    assert(small_bin_index == SMALLBINS_INDEX);
    auto index = bit_index;

    Chunk *chunk_list{nullptr};
    while(true) {
        chunk_list = &small_chunks[index];
        if(isPointingToSelf(*chunk_list) || !isBinBitIndexSet(small_bin_index, index)) {
            unsetBitIndex(small_bin_index, index);
            index++;
        }else {
            Chunk *match  = chunk_list->getPrev();
            unlinkChunk(match);
            assert(match != nullptr);
            return match;
        }

        // Not sure how malloc does this, but probably good idea to restrict this to one above
        // if there is no exact match, otherwise we are wasting a lot of memory space
        if(index > getMaxSmallBinBitIndex() || index - bit_index >= 2 ) {
            return nullptr;
        }
    }
}

Chunk *tryFindLargeChunk(Chunk *large_chunks, std::size_t size) {
    Chunk *current = large_chunks->getPrev();
    while(large_chunks != current) {
        if (current->getSize() >= size) {
            unlinkChunk(current);
            // TODO prepare chunk to be returned to user
            // Also split the chunk maybe
            return current;
        }
        current = current->getPrev();
    }
    return nullptr;
}

bool isPointingToSelf(const Chunk &list_head) {
    // Basic check that if next points to self, previous should too
    if(list_head.getNext() == &list_head) {
        assert(list_head.getPrev() == &list_head);
    }
    // or if previous points to self, next should too
    if(list_head.getPrev() == &list_head) {
        assert(list_head.getNext() == &list_head);
    }
    return list_head.getPrev() == &list_head && list_head.getNext() == &list_head;
}

bool hasElementsInList(const Chunk &list_head) {
    return !isPointingToSelf(list_head);
}


void *AfMalloc::malloc(std::size_t size) {

    std::size_t needed_size =  getMallocNeededSize(size);

    // if there are free chunks, try to use them
    if(hasElementsInList(af_arena_.unsorted_chunks_)) {
        if(auto maybe_chunk = findChunkFromUnsortedFreeChunks(needed_size)) {
            return *maybe_chunk;
        }
    }
    if(isInFastBinRange(needed_size)) {
        if(auto *chunk = tryFindFastBinChunk(needed_size)) {
            return chunk;
        }
    }
    if(isInSmallBinRange(needed_size)) {
        if(auto *chunk = tryFindSmallBinChunk(needed_size)) {
            return chunk;
        }
    }
    if(!isPointingToSelf(af_arena_.unsorted_large_chunks_)) {
        if(auto *chunk = tryFindLargeChunk(&af_arena_.unsorted_large_chunks_, needed_size)) {
            return chunk;
        }
    }

    // if there are no free chunks, and we have no enough size, we need to allocate a new block
    // If there is less then HEAD_OF_CHUNK_SIZE left, we need to
    if(static_cast<long>(af_arena_.free_size_) - static_cast<long>(HEAD_OF_CHUNK_SIZE) < static_cast<long>(needed_size)) {
        if(needed_size > MAX_HEAP_SIZE) {
            assert(false); // unsupported case
        }
        if(af_arena_.top_ != nullptr) {
            assert(false); // unsupported case with missing new heap
        }
        // allocate block of memory, I am not sure if this block is aligned on anything other than page size
        void *p1 = MMAP (nullptr, MAX_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE);
        if(p1 == nullptr) {
            return nullptr;
        }
        assert(get_alignment_size(p1, 4096) == 0); // page size is 4096 bytes or 4kB
        std::cout << "Value is rounded up on page size: " << reinterpret_cast<uint64_t>(p1) << "" << reinterpret_cast<uint64_t>(p1)/4096 << std::endl;
        af_arena_.allocated_size_ = MAX_HEAP_SIZE;
        af_arena_.begin_ = p1;
        af_arena_.top_= p1;
        af_arena_.free_size_ = MAX_HEAP_SIZE;
    }

    // We can store anything which has alignment of 16 bytes.

    // Here we will give to user the size needed
    // what we will do is we will return to user pointer after chunk's block
    void *user_ptr = af_arena_.top_;


    auto *user_chunk = std::construct_at(static_cast<Chunk*>(user_ptr), 0, needed_size, nullptr, nullptr);

    af_arena_.free_size_ -=  needed_size;
    af_arena_.top_ = moveToTheNextPlaceInMem(user_chunk, needed_size);


    return moveToTheNextPlaceInMem(user_ptr, HEAD_OF_CHUNK_SIZE);
}

void *AfMalloc::memAlign([[maybe_unused]] std::size_t alignment, [[maybe_unused]] std::size_t size) {
    return nullptr;
}