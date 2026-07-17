#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <memory>
#include <cstring>
#include <optional>
#include <algorithm>

#include "AfMalloc.hpp"

#include <numeric>
#include <sys/mman.h>

#define MMAP(addr, size, prot, flags) \
 mmap(addr, (size), (prot), (flags)|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)


static_assert(sizeof(long long int) == 8);





void* moveToTheNextPlaceInMem(void *ptr, std::size_t size) {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + size);
}

void* moveToThePreviousPlaceInMem(void *ptr, std::size_t size) {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) - size);
}


Chunk *getChunkPointerBefore(void *ptr, std::size_t size) {
    return std::launder(reinterpret_cast<Chunk*>(reinterpret_cast<uintptr_t>(ptr) - size));
}

Chunk *getChunkPointerAfter(void *ptr, std::size_t size) {
    return std::launder(reinterpret_cast<Chunk*>(reinterpret_cast<uintptr_t>(ptr) + size));
}

Chunk *chunkAt(void *ptr) {
    return std::launder(static_cast<Chunk*>(ptr));
}

Flag getFlag(const bool isPrevFree) {
    return isPrevFree? Flag{PREV_FREE} : Flag{EMPTY_FLAG};
}
void* moveToTheNextPlaceInMemAlignment(void *ptr, std::size_t size, std::size_t alignment) {
    auto *new_place =  reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + size);
    auto additional_alignment_movement = getAlignmentSize(new_place, alignment);
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(new_place) + additional_alignment_movement);
}


std::size_t getNeededSizeWithAlignment(void *ptr, std::size_t alignment, std::size_t size) {
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



    /// chunk when used
    /// ----|+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    ///  a) prev_size
    ///  ----|+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-  ---
    ///  b) current_size
    ///  ----|+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-   --- from here
    ///  c) user memory
    ///  .....
    ///  ----|+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    ///  ----start of new chunk---
    ///  A1) prev_size
    ///  ----|+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-    --- till here, we can use all that memory
    ///
    ///   prev_size memory is not set if the previous chunk is used
    ///   and we can check if previous chunk is used by checking last few bits of our current_size
    ///
    ///
    return (size + SIZE_OF_SIZE + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;
}


std::size_t getMallocNeededSize(const std::size_t size, const std::size_t alignment) {
    assert(alignment % ALIGNMENT == 0);
    if (size + SIZE_OF_SIZE <= CHUNK_SIZE) {
        return CHUNK_SIZE;
    }
    const auto alignmentMask = alignment - 1;

    return (size + SIZE_OF_SIZE + alignmentMask) & ~alignmentMask;
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


AfArena::AfArena(const std::size_t id) : bin_indexes_(2), last_heap_(nullptr), id_(id) {
    // TODO eat own dog food for bin indexes?

    initializeAfArena();
}

void AfArena::initializeAfArena() {
    fast_chunks_.resize(NUM_FAST_CHUNKS, {0, 0, nullptr, nullptr});
    std::ranges::for_each(fast_chunks_, [this](Chunk &chunk) {
        if(getGlobalConfig().isTrackingGlobal) {
            //createPtrHumaneReadableName("fast_chunk_", &chunk);
        }
        chunk.setNext(&chunk);
        chunk.setPrev(&chunk);
    });

    small_chunks_.resize(NUM_SMALL_CHUNKS, {0, 0, nullptr, nullptr});
    std::ranges::for_each(small_chunks_, [this](auto &chunk) {
        // if(globalConfig.isTrackingGlobal) {
        //     //createPtrHumaneReadableName("small_chunk_", &chunk);
        // }
        chunk.setNext(&chunk);
        chunk.setPrev(&chunk);
    });
    // Set chunks to point to itself
    unsorted_large_chunks_ = {0, 0, nullptr, nullptr};
    unsorted_large_chunks_.setNext(&unsorted_large_chunks_);
    unsorted_large_chunks_.setPrev(&unsorted_large_chunks_);

    // if(globalConfig.isTrackingGlobal) {
    //     //createPtrHumaneReadableName("unsorted_large_chunks_", &af_main_arena_.unsorted_large_chunks_);
    // }

    unsorted_chunks_ = {0, 0, nullptr, nullptr};
    unsorted_chunks_.setNext(&unsorted_chunks_);
    unsorted_chunks_.setPrev(&unsorted_chunks_);

    // if(globalConfig.isTrackingGlobal) {
    //     //createPtrHumaneReadableName("unsorted_chunks_", &unsorted_chunks_);
    // }


    // only FAST and SMALL bin indexes live here
    bin_indexes_.resize(2, {0});
}


void AfArena::moveToUnsortedLargeChunks(Chunk *free_chunk) {
    Chunk *next_chunk = unsorted_large_chunks_.getNext();

    free_chunk->setNext(next_chunk);
    next_chunk->setPrev(free_chunk);


    free_chunk->setPrev(&unsorted_large_chunks_);
    unsorted_large_chunks_.setNext(free_chunk);

}

void AfArena::moveToFastBinsChunks(Chunk *free_chunk, std::size_t bit_index) {
    auto &fast_bin_head = fast_chunks_[bit_index];
    auto *next = fast_bin_head.getNext();
    fast_bin_head.setNext(free_chunk);

    free_chunk->setPrev(&fast_bin_head);
    free_chunk->setNext(next);

    next->setPrev(free_chunk);
}

void AfArena::moveToSmallBinsChunks(Chunk *free_chunk, std::size_t bit_index) {
    auto &small_bin_head = small_chunks_[bit_index];
    auto *next = small_bin_head.getNext();
    small_bin_head.setNext(free_chunk);

    free_chunk->setPrev(&small_bin_head);
    free_chunk->setNext(next);

    next->setPrev(free_chunk);
}

void AfArena::extendTopChunk(){
    auto *top_chunk = reinterpret_cast<Chunk *>(getTop());
    assert(top_chunk->isPrevFree());
    Chunk *prev_chunk = getChunkPointerBefore(top_chunk, top_chunk->getPrevSize());

    prev_chunk->setSizeWithFlag(top_chunk->getSize() + prev_chunk->getSize(), getFlag(prev_chunk->isPrevFree()));
    prev_chunk->unsetPrevFree();

    top_ = prev_chunk;
    // is this good usage of start of lifetime of chunk
    clearUpDataSpaceOfChunk(top_chunk);
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

    chunk->setNext(nullptr);
    chunk->setPrev(nullptr);
}

AfMalloc::AfMalloc(const std::size_t num_arenas) : num_arenas_(num_arenas) {
    init();
}

AfMalloc::AfMalloc() {
    init();

}

AfMalloc::AfMalloc(bool track_pointers) :track_pointers_(track_pointers) {
    init();
}

bool AfArena::isTopChunk(Chunk *chunk) {
    return std::addressof(*std::launder(reinterpret_cast<Chunk*>(top_))) == std::addressof(*chunk);
}

void AfMalloc::init() {
    // TODO this should be implemented that we allocate memory with our own allocator and size
    for (int i = 1; i < num_arenas_ + 1; i++) {
        arenas_.emplace_back(std::make_unique<AfArena>(i));
    }


}


// Once deallocation is done, we write to the next chunk that prev_size is our size current, and we write to
// ourselves that we are not in use

// That should enable merging of two chunks
void AfMalloc::free(void *p) {
    // What guarantees we have here
    // How do we want our merging of top chunk to work?

    // LifetimeCheck: chunk is created
    Chunk *free_chunk = getChunkPointerBefore(p, HEAD_OF_CHUNK_SIZE);

    AfHeap *heap = getHeapAddress(p);
    AfArena *arena = heap->arena_ptr_;
    assert(arena != nullptr);

    std::unique_lock lock{arena->arena_lock_};


    // after we have heap we need to get arena
    // then work with it
    // we can get heap by doing aligning our chunk to HEAP_SIZE
    // we know that each of our chunks in heap will be N*HEAP_SIZE + chunk_offset

    clearUpDataSpaceOfChunk(free_chunk);

    auto const isFreedChunkCoalescable = isChunkCoalescable(*free_chunk);

    if (!isFreedChunkCoalescable) {
        // link it and go back
    }

    // Here we want to check if the chunk in the physical memory before us has actually
    // been freed. If so, we can try to merge those two


    // here we are reading only the memory from objects whose lifetime has started
    if(isFreedChunkCoalescable && free_chunk->isPrevFree()) {
        // We are reading from prev_chunk getPrevSize only if free_chunk isPrevFree
        // This way we do not want to std::launder on address in use with another object
        Chunk *prev_chunk = getChunkPointerBefore(free_chunk, free_chunk->getPrevSize());
        if (isChunkCoalescable(*prev_chunk)) {
            // previous chunk is free, we need to merge them

            unlinkChunk(prev_chunk);

            prev_chunk->setSizeWithFlag(free_chunk->getPrevSize() + free_chunk->getSize(), getFlag(prev_chunk->isPrevFree()));
            free_chunk = prev_chunk;
            clearUpDataSpaceOfChunk(free_chunk);
        }

    }


    // Step 2 is to merge next chunk if possible

    // LifetimeCheck: This chunk's lifetime should also be fine. Chunk is either top or some other chunk created
    Chunk *next_chunk = getChunkPointerAfter(free_chunk, free_chunk->getSize());

    // comparing address space


    // Our chunk is not free until now. It can't be as we are only merging it now.
    assert(!next_chunk->isPrevFree());

    // To coalesce nextChunk we need to check if next chunk is topChunk
    // or it is possible to coalesce it
    bool isNextChunkFree{false};
    // next chunk is not top, hence we need to check by moving to the chunk after next chunk if the next chunk is free
    /// [free_chunk][next_chunk][(prev_size)(size | is_prev_free) chunk after next_chunk]
    if (!arena->isTopChunk(next_chunk)) {
        // LifetimeCheck: should be fine. Again chunk's lifetime has started on malloc
        isNextChunkFree = getChunkPointerAfter(next_chunk, next_chunk->getSize())->isPrevFree();
    }

    bool canNextChunkBeCoalesced = isChunkCoalescable(*next_chunk) && isNextChunkFree;


    // if the next_chunk is free, we will merge the `freeChunk` and the `nextChunk`
    // otherwise `nextChunk` is allocated, and we can't merge these two

    // This case can happen that we have fast_chunk and top_chunk - in that case we can't merge them
    if(isFreedChunkCoalescable && canNextChunkBeCoalesced) {
        // nextChunk is free so we need to merge that one too
        free_chunk->setSizeWithFlag(free_chunk->getSize() + next_chunk->getSize(), getFlag(free_chunk->isPrevFree()));

        unlinkChunk(next_chunk);

        clearUpDataSpaceOfChunk(free_chunk);
        Chunk *new_next_chunk = getChunkPointerAfter(free_chunk, free_chunk->getSize());
        // LifetimeCheck: at this point we are materializing object of previous_size_
        // which might have been used by user's object. However, that should be fine
        // because user called free, and now user's object lifetime has ended
        new_next_chunk->setPrevFree();
        new_next_chunk->setPrevSize(free_chunk->getSize());
    }else {
        next_chunk->setPrevFree();
        next_chunk->setPrevSize(free_chunk->getSize());
    }

    // when the nextChunk is free, if chunkTwoHopsInFront was free,
    // it would be merged in the step before.
    // This way we know that our chunk is free, and only one step around us can be free

    // We need to find where is the next chunk, as we might have merged it in the step before
    // This is now a new pointer
    next_chunk = getChunkPointerAfter(free_chunk, free_chunk->getSize());


    // If chunk and top are part of same heap, then we extend heap chunk, otherwise no
    if(isFreedChunkCoalescable &&  arena->isTopChunk(next_chunk)) {
        // next chunk here is arena.top_
        assert(next_chunk == arena->top_);
        // in this part of code we extend top to the free_chunk
        // this means we can't add free chunk to the free list

        // here we should actually merge our chunk with the top, and that way we have extended the unlimited free chunk
        arena->extendTopChunk();
        // We have extended the top, the rest of the code deals with adding the chunk to the unsorted chunks
        return;
    }

    arena->linkChunkToUnsortedChunks(free_chunk);
}

AfMalloc::~AfMalloc() {
    //munmap(af_main_arena_.begin_, af_main_arena_.allocated_size_);
}

void AfArena::moveChunkToCorrectBin(Chunk *current_chunk, std::size_t needed_size) {
    auto maybe_bin_index = findBinIndex(needed_size);
    // free_chunk_list -> 1 -> 2 - > 3
    if(!maybe_bin_index) {
        moveToUnsortedLargeChunks(current_chunk);
    }else {
        if(auto [index, bit_index] = *maybe_bin_index; index == FASTBINS_INDEX) {
            // fast range
            moveToFastBinsChunks(current_chunk, bit_index);
            setBinIndex(index, bit_index);
        }else {
            // small range
            moveToSmallBinsChunks(current_chunk, bit_index);
            setBinIndex(index, bit_index);
        }
    }

}

void* AfArena::findChunkFromUnsortedFreeChunks(std::size_t needed_size) {
    // Next to the free chunk, unless it is in the fast bin range, there will always be an allocated chunk,
    // since otherwise we would coalesce them
    // on the free.
    // For the fast bin chunk, even if the chunk next to the fast bin chunk is free, we would not coalesce them.

    // Unsorted free chunks are stored in a double linked list
    Chunk *start  = &unsorted_chunks_;
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
        Chunk *next_chunk = current_chunk->getNext();
        unlinkChunk(current_chunk);
        moveChunkToCorrectBin(current_chunk, current_chunk->getSize());
        current_chunk = next_chunk;
    }

    if(match == nullptr) {
        return nullptr;
    }

    unlinkChunk(match);

    // TODO this is opportunity to split the chunk on the multiple chunks, since we could otherwise get really
    // big chunk
    Chunk* next_chunk = getChunkPointerAfter(match, match->getSize());
    // next chunk only knows that we are free
    next_chunk->unsetPrevFree();
    // this part of memory will be used by our chunk also, hence we need to zero the memory
    next_chunk->setPrevSize(0x0000);

    return match;
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


void AfArena::setBinIndex(std::size_t bin, std::size_t bit) {
    bin_indexes_[bin] |= (1ul << bit);
}

void AfArena::unsetBitIndex(std::size_t bin, std::size_t bit) {
    bin_indexes_[bin] &= ~(1ul << bit);
}

bool AfArena::isBinBitIndexSet(std::size_t bin, std::size_t bit) {
    return bin_indexes_[bin].test(bit);
}

/**
 * Fast bin chunk gets allocated, we remove it from the list
 * Later we add it to the unsorted chunks, and insert back into the free list
 * It is never coalesced however.
 * @param size
 * @return
 */
Chunk *AfArena::tryFindFastBinChunk(const std::size_t size) {
    auto [fast_bin_index, bit_index] = *findBinIndex(size);
    assert(fast_bin_index == FASTBINS_INDEX);
    auto index = bit_index;

    // Traverse only up to 2 blocks away from our chunk
    while(index < getMaxFastBinBitIndex() && (index - bit_index <= 2)) {
        Chunk &chunk_list = fast_chunks_[index];
        if(isPointingToSelf(chunk_list)) {
            unsetBitIndex(fast_bin_index, bit_index);
        }else {
            // Not sure how malloc does this, but probably good idea to restrict this to one above
            // if there is no exact match, otherwise we are wasting a lot of memory space
            Chunk *match = chunk_list.getNext();
            unlinkChunk(match);
            assert(match != nullptr);
            Chunk *next_chunk = getChunkPointerAfter(match, match->getSize());
            // next chunk only knows that we are free
            next_chunk->unsetPrevFree();
            // this part of memory will be used by our chunk also, hence we need to zero the memory
            next_chunk->setPrevSize(0x0000);
            return match;
        }
        index++;
    }
    return nullptr;
}


void AfArena::linkChunkToUnsortedChunks(Chunk *chunk) {
    // We append to the top of the list newly freed chunk
    Chunk *head_chunk = &unsorted_chunks_;
    if(isPointingToSelf(*head_chunk)) {
        head_chunk->setNext(chunk);
        head_chunk->setPrev(chunk);

        chunk->setNext(head_chunk);
        chunk->setPrev(head_chunk);
        return;
    }

    Chunk *last_in_chunk = head_chunk->getNext();

    chunk->setPrev(head_chunk);
    chunk->setNext(last_in_chunk);

    last_in_chunk->setPrev(chunk);
    head_chunk->setNext(chunk);
}

/**
 * Try to find chunk which is of same size, or one size larger.
 * Not sure if we should iterate more here, and then just split the chunk if found
 * @param size
 * @return
 */
Chunk *AfArena::tryFindSmallBinChunk(std::size_t size) {
    std::vector<Chunk > &small_chunks = small_chunks_;
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
            Chunk* next_chunk = getChunkPointerAfter(match, match->getSize());
            // next chunk only knows that we are free
            next_chunk->unsetPrevFree();
            // this part of memory will be used by our chunk also, hence we need to zero the memory
            next_chunk->setPrevSize(0x0000);
            return match;
        }

        // Not sure how malloc does this, but probably good idea to restrict this to one above
        // if there is no exact match, otherwise we are wasting a lot of memory space
        if(index > getMaxSmallBinBitIndex() || index - bit_index >= 2 ) {
            return nullptr;
        }
    }
}

AfHeap* AfArena::setupHeap(void *p) {
    assert(getAlignmentSize(p, MAX_HEAP_SIZE) == 0);

    auto *heap = std::construct_at(static_cast<AfHeap*>(p), MAX_HEAP_SIZE, this, last_heap_);
    last_heap_ = heap;

    // should be moved to setupNewHeap
    top_ = moveToTheNextPlaceInMemAlignment(last_heap_, sizeof(AfHeap), ALIGNMENT);

    allocated_size_ += last_heap_->heap_size_;

    // now we need to put that new top is this one from this heap

    return heap;
}

Chunk *tryFindLargeChunk(Chunk *large_chunks, std::size_t size) {
    Chunk *current = large_chunks->getPrev();
    while(large_chunks != current) {
        if (current->getSize() >= size) {
            unlinkChunk(current);
            // TODO prepare chunk to be returned to user
            // Also split the chunk maybe
            Chunk* next_chunk = getChunkPointerAfter(current, current->getSize());
            // next chunk only knows that we are free
            next_chunk->unsetPrevFree();
            // this part of memory will be used by our chunk also, hence we need to zero the memory
            next_chunk->setPrevSize(0x0000);
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

// TODO rename
bool hasChunkInList(const Chunk &list_head) {
    return !isPointingToSelf(list_head);
}


AfArena& AfMalloc::getArena() {
    // get number active arenas
    // get max number arenas
    // if this is not a new thread
    //      try to get last active arena for this thread
    //      (this should improve locality)
    // if there is less active arenas then we have allocated
    //      return that one
    //  if there is space to create one more arena
    //        create new one and use that one
    // lock arena and return it
    if (getGlobalConfig().disableRoundRobin) {
        return *arenas_[0].get();
    }
    if (threadStaticStruct.arena_ != nullptr) {
        return *threadStaticStruct.arena_;
    }

    const auto arena_index = next_arena_.fetch_add(1) % arenas_.size();
    auto &arena = arenas_[arena_index];
    threadStaticStruct.arena_ = arena.get();
    return *threadStaticStruct.arena_;
}

// todo rename to allocate new heap block and then we don't have to pass arena
void* AfMalloc::allocateNewHeap() {
    void *p1 = MMAP (nullptr, MAX_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE);
    if(p1 == nullptr) {
        return nullptr;
    }

    if (getAlignmentSize(p1, MAX_HEAP_SIZE) != 0) {
        const auto ok = munmap(p1, MAX_HEAP_SIZE);
        assert(ok == 0);
        p1 = MMAP (nullptr, 2 * MAX_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE);
        if (p1 == nullptr) {
            return nullptr;
        }
        // now we have a chunk of memory which is
        //       |32-aligned [p1 xxxx]|[p2     xxxx            ]|[p3xxxx] ....]
        // we need to free anything before p2, and after p3
        auto alignment = getAlignmentSize(p1, MAX_HEAP_SIZE);
        void *p2 = moveToTheNextPlaceInMem(p1, alignment);
        void *p3 = moveToTheNextPlaceInMem(p2, MAX_HEAP_SIZE);
        munmap(p1, alignment);
        munmap(p3, MAX_HEAP_SIZE - alignment);
        p1 = p2;
    }

    // https://www.youtube.com/watch?v=pbkQG09grFw


    return p1;
}

void *AfMalloc::malloc(const std::size_t size) {
    /**
     * AfMalloc first aquires the pointer to arena.
     *
     * After arena is aquired, we can check if there are some chunks which are recently freed that we can reuse.
     * Then we are checking if chunk is in fast bin range, small bin range or in unsorted larger chunks.
     */
    AfArena &af_arena = getArena();

    std::unique_lock lock(af_arena.arena_lock_);


    const std::size_t malloc_needed_size =  getMallocNeededSize(size);

    // if there are free chunks, try to use them
    if(hasChunkInList(af_arena.unsorted_chunks_)) {
        auto *chunk = af_arena.findChunkFromUnsortedFreeChunks(malloc_needed_size);
        if (chunk) {
            return moveToTheNextPlaceInMem(chunk, HEAD_OF_CHUNK_SIZE);;
        }
    }

    if(isInFastBinRange(malloc_needed_size)) {
        if(auto *chunk = af_arena.tryFindFastBinChunk(malloc_needed_size)) {
            return moveToTheNextPlaceInMem(chunk, HEAD_OF_CHUNK_SIZE);
        }
    }

    if(isInSmallBinRange(malloc_needed_size)) {
        if(auto *chunk = af_arena.tryFindSmallBinChunk(malloc_needed_size)) {
            return moveToTheNextPlaceInMem(chunk, HEAD_OF_CHUNK_SIZE);
        }
    }

    if(!isPointingToSelf(af_arena.unsorted_large_chunks_)) {
        if(auto *chunk = tryFindLargeChunk(&af_arena.unsorted_large_chunks_, malloc_needed_size)) {
            return moveToTheNextPlaceInMem(chunk, HEAD_OF_CHUNK_SIZE);
        }
    }

    // if there are no free chunks, and we have no enough size, we need to allocate a new block
    // If there is less then HEAD_OF_CHUNK_SIZE left, we need to

    // There needs to be enough size for our allocation and for the top chunk left
    if(af_arena.getTopChunkSize()  < malloc_needed_size + TOP_CHUNK_NEEDED_SIZE) {
        if(malloc_needed_size > MAX_HEAP_SIZE) {
            assert(false); // unsupported case
        }


        if(af_arena.top_ != nullptr) {
            // The leftover chunk should be reused
            assert(false);
        }

        AfHeap *af_heap{nullptr};
        if(void *heap_memory = allocateNewHeap(); heap_memory != nullptr) {
            af_heap = af_arena.setupHeap(heap_memory);
        }


        auto* top =  std::start_lifetime_as<Chunk>(af_arena.top_);
        assert(af_heap != nullptr);
        top->setSizeWithFlag(af_heap->heap_size_ - getPtrDiffSize(af_arena.top_, af_arena.last_heap_), Flag{EMPTY_FLAG}) ;
    }


    // Here we will give to user the size needed
    // what we will do is we will return to user pointer after chunk's block
    void *user_ptr = af_arena.top_;


    auto *user_chunk = chunkAt(user_ptr);
    auto *old_top = user_chunk;
    const std::size_t top_chunk_size = old_top->getSize();


    user_chunk->setSizeWithFlag(malloc_needed_size, getFlag(user_chunk->isPrevFree()));
    clearUpDataSpaceOfChunk(user_chunk);

    if(track_pointers_) {
        createPtrHumaneReadableName("ptr", user_chunk);
    }


    af_arena.top_ = moveToTheNextPlaceInMem(user_chunk, malloc_needed_size);


    auto *top = std::start_lifetime_as<Chunk>(af_arena.top_);
    top->setSizeWithFlag(top_chunk_size - malloc_needed_size, Flag(EMPTY_FLAG));


    return moveToTheNextPlaceInMem(user_ptr, HEAD_OF_CHUNK_SIZE);
}

/**
 *
 * @param second Pointer from which to reduce first
 * @param first pointer which reduces first
 * @return difference between second and first
 */
std::size_t getPtrDiffSize(void *second, void *first) {
    return reinterpret_cast<uintptr_t>(second) - reinterpret_cast<uintptr_t>(first);
}

void *AfMalloc::memAlign(std::size_t alignment, std::size_t size) {
    // alignment + size
    assert(alignment != 0);
    // assert that it is power of 2
    assert((alignment & (-alignment)) == alignment);

    // if alignment is not at least 16, reconfigure to multiple of 16
    // if alignment or size which we allocate for chunk is not multiple of ALIGNMENT
    // then we will have problem allocating new chunk - either the size of our chunk needs to increase
    // or the chunk next one will be misaligned


    alignment = std::max(alignment, ALIGNMENT);

    // mallocNeededSize would not work correctly if we pass our needed alignment
    // It would allocate more memory than needed and we would waste space
    // This would result in memory consumption issue
    // Reasoning - example is if user wants to allocate 25 bytes on 32 byte alignment
    // with alignment of 16 -> 25 + 15 + 8 = 48 -> 48/ 16 = 3
    // with alignment of 32 -> 25 + 31 + 8 = 64 / 32 =2 -> use 64 bytes
    // malloc needed size needs to be aligned on 16 bytes conceptually, not on bytes user requested
    // the start of user data needs to be aligned on user size, and that is a different concept.
    // By forcing alignment of chunk on the user size, we still have issue that user start might not be aligned on
    // the same alignment space.
    // For example - let say user wants to align on 64.
    // If our chunk is aligned also on 64, this means user data will start on 64 + 16(prev_size + size) meaning
    // we will be out of sync with what user wants - 80byte offset is not 64 aligned
    // It is still good idea to keep our size 16 byte aligned so we can move correctly from one chunk to the other
    const auto allocatedSizeNeeded = getMallocNeededSize(size);

    auto &arena = getArena();
    Chunk *top_chunk = std::launder(reinterpret_cast<Chunk*>(arena.getTop()));
    std::size_t top_chunk_size = top_chunk->getSize();

    std::size_t missAlignedSpace = getAlignmentSize(top_chunk, alignment);
    void *start_of_user_data = moveToTheNextPlaceInMem(top_chunk, missAlignedSpace);

    // since start_of_user_data is now aligned to what user wants
    // we need to check if have 16 bytes before to fit our HEAD_OF_CHUNK and that we can fit one whole chunk before
    // -- there are two options here - we can fit the exactly HEAD_OF_CHUNK before start_of_user_data
    // and there is no additional space left
    // -----if there is some space left, it needs to be at least CHUNK_SIZE
    while(start_of_user_data < moveToTheNextPlaceInMem(top_chunk,top_chunk->getSize())) {
        const auto diff = getPtrDiffSize(start_of_user_data, top_chunk);
        if (diff < HEAD_OF_CHUNK_SIZE) {
            start_of_user_data = moveToTheNextPlaceInMem(start_of_user_data, alignment); // is this ok
        }
        else if (diff == HEAD_OF_CHUNK_SIZE) {
            break;
        }
        else if (diff > HEAD_OF_CHUNK_SIZE && diff <  CHUNK_SIZE) {
            start_of_user_data = moveToTheNextPlaceInMem(start_of_user_data, alignment);
        }
        else{
            break;
        }
    }

    assert(getAlignmentSize(start_of_user_data, alignment) == 0);
    assert(getPtrDiffSize(start_of_user_data, top_chunk) > HEAD_OF_CHUNK_SIZE);

    void *start_of_chunk = moveToThePreviousPlaceInMem(start_of_user_data, HEAD_OF_CHUNK_SIZE);

    // we need to deal with case when between our chunk and previous is less than 32 bytes
    // no chunk can fit then and we have an issue
    std::size_t unusedChunkSize  = getPtrDiffSize(start_of_chunk, top_chunk);

    top_chunk_size-= unusedChunkSize;

    if (unusedChunkSize == 0) {
        // pass
    }else {
        assert(unusedChunkSize >= CHUNK_SIZE);
        assert(unusedChunkSize % ALIGNMENT == 0); // it is a multiple of 16


        auto *unusedChunk = std::launder(reinterpret_cast<Chunk*>(arena.getTop()));

        const auto prevSize = unusedChunk->getPrevSize();
        unusedChunk->setSizeWithFlag(unusedChunkSize, getFlag(unusedChunk->isPrevFree()));

        unusedChunk->setPrevSize(prevSize);

        arena.linkChunkToUnsortedChunks(unusedChunk);
    }

    assert(reinterpret_cast<uintptr_t>(start_of_user_data) % alignment == 0);

    Chunk * chunk = std::construct_at(static_cast<Chunk*>(start_of_chunk), 0, 0, nullptr, nullptr);
    if (unusedChunkSize) {
        chunk->setPrevSize(unusedChunkSize);
    }

    chunk->setSizeWithFlag(allocatedSizeNeeded, getFlag(unusedChunkSize > 0));


    top_chunk_size-=allocatedSizeNeeded;

    arena.top_ = moveToTheNextPlaceInMem(start_of_chunk, allocatedSizeNeeded);
    auto *newTopChunk = std::start_lifetime_as<Chunk>(arena.top_);
    newTopChunk->setSizeWithFlag(top_chunk_size, Flag{EMPTY_FLAG});

    return moveToTheNextPlaceInMem(chunk, HEAD_OF_CHUNK_SIZE);
}

void AfMalloc::dumpMemory() {
    // std::cout << std::format("-------Dumping unsorted chunks-------") << std::endl;
    //
    // {
    //     Chunk &head = af_main_arena_.unsorted_chunks_;
    //     Chunk *start = head.getNext();
    //
    //     std::cout << std::format("{}[{} {} {} {}]", getPtrHumaneReadableName(&head), start->getPrevSize(), start->getSize(), getPtrHumaneReadableName(head.getNext()), getPtrHumaneReadableName(head.getPrev())) << std::endl;
    //     while(start != &head) {
    //         std::cout << std::format("{}[{} {} {} {}]", getPtrHumaneReadableName(start), start->getPrevSize(), start->getSize(), getPtrHumaneReadableName(start->getNext()), getPtrHumaneReadableName(start->getPrev())) << std::endl;
    //         start = start->getNext();
    //     }
    // }
    //
    // {
    //     auto &fast_chunks = af_main_arena_.fast_chunks_;
    //     std::size_t i{0};
    //     for(auto &head: fast_chunks) {
    //         if(!isPointingToSelf(head)) {
    //             std::cout << std::format("-------FastChunk{}-------", i++) << std::endl;
    //             Chunk *start = head.getNext();
    //
    //             std::cout << std::format("{}[{} {} {} {}]", getPtrHumaneReadableName(&head), start->getPrevSize(), start->getSize(), getPtrHumaneReadableName(head.getNext()), getPtrHumaneReadableName(head.getPrev())) << std::endl;
    //             while(start != &head) {
    //                 std::cout << std::format("{}[{} {} {} {}]", getPtrHumaneReadableName(start), start->getPrevSize(), start->getSize(), getPtrHumaneReadableName(start->getNext()), getPtrHumaneReadableName(start->getPrev())) << std::endl;
    //                 start = start->getNext();
    //             }
    //         }else {
    //             i++;
    //         }
    //     }
    // }
    //
    // {
    //     auto &small_chunks = af_main_arena_.small_chunks_;
    //     std::size_t i{0};
    //     for(auto &head: small_chunks) {
    //         if(!isPointingToSelf(head)) {
    //             std::cout << std::format("-------SmallChunk{}-------", i++) << std::endl;
    //             Chunk *start = head.getNext();
    //
    //             std::cout << std::format("{}[{} {} {} {}]", getPtrHumaneReadableName(&head), start->getPrevSize(), start->getSize(), getPtrHumaneReadableName(head.getNext()), getPtrHumaneReadableName(head.getPrev())) << std::endl;
    //             while(start != &head) {
    //                 std::cout << std::format("{}[{} {} {} {}]", getPtrHumaneReadableName(start), start->getPrevSize(), start->getSize(), getPtrHumaneReadableName(start->getNext()), getPtrHumaneReadableName(start->getPrev())) << std::endl;
    //                 start = start->getNext();
    //             }
    //         }else {
    //             i++;
    //         }
    //     }
    // }

}
