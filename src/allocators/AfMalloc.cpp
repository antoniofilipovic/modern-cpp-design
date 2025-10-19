#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <memory>
#include <cstring>
#include <optional>

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


AfArena::AfArena() : bin_indexes_(2) {
    // TODO eat own dog food for bin indexes?
}


void AfMalloc::moveToUnsortedLargeChunks(Chunk *free_chunk) {

}

void AfMalloc::moveToFastBinsChunks(Chunk *free_chunk, std::size_t bit_index) {

}

void AfMalloc::moveToSmallBinsChunks(Chunk *free_chunk, std::size_t bit_index) {

}

void AfMalloc::extendTopChunk(){
    auto *top_chunk = static_cast<Chunk *>(getTop());
    assert(top_chunk->isPrevFree());
    Chunk *prev_chunk = moveToThePreviousChunk(top_chunk, top_chunk->getPrevSize());
    removeFromFreeChunks(prev_chunk);
    clearUpDataSpaceOfChunk(prev_chunk);
    af_arena_.top_ = prev_chunk;
    prev_chunk->unsetPrevFree();
    prev_chunk->setSize(0);
}

void AfMalloc::removeFromFreeChunks(Chunk* chunk) {
    Chunk *freeChunkLinkedList = af_arena_.unsorted_chunks_;
    if(freeChunkLinkedList == nullptr) {
        return;
    }
    bool isIn{false};
    do {
        if(freeChunkLinkedList == chunk) {
            isIn = true;
            break;
        }
        freeChunkLinkedList = freeChunkLinkedList->getNext();
    }while(freeChunkLinkedList != nullptr && freeChunkLinkedList != af_arena_.unsorted_chunks_);
    assert(isIn);
    auto *nextChunk = chunk->getNext();
    auto *prevChunk = chunk->getPrev();
    // in case chunk points on itself
    if(nextChunk == prevChunk && nextChunk == chunk) {
        af_arena_.unsorted_chunks_ = nullptr;
        return;
    }

    // 2<--1<->2-->1
    nextChunk->setPrev(prevChunk);
    prevChunk->setNext(nextChunk);

    if(af_arena_.unsorted_chunks_ == chunk) {
        af_arena_.unsorted_chunks_ = nextChunk;
    }
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
    if(free_chunk->isPrevFree()) {
        // previous chunk is free, we need to merge them
        std::size_t prev_size = free_chunk->getPrevSize();

        /// Then we need to go to that place in the memory
        auto *chunk_before = moveToThePreviousChunk(free_chunk, prev_size);
        // if we don't do this here, then we will later have a problem
        // with merging two chunks and iterating over free chunks because of zeroing of memory
        removeFromFreeChunks(chunk_before);
        chunk_before->setSize( prev_size + free_chunk->getSize());
        free_chunk = chunk_before;
        clearUpDataSpaceOfChunk(free_chunk);
    }

    auto *next_chunk = moveToTheNextChunk(free_chunk, free_chunk->getSize());
    // Small assert to check our chunk is not free until now. It can't be as we are only merging it now.
    assert(!next_chunk->isPrevFree());

    // This works even if we are at the at top, as on the top chunk we don't write size
    Chunk *chunk_two_hops_in_front =  moveToTheNextChunk(next_chunk, next_chunk->getSize());

    // if the next_chunk is free, we will merge the `freeChunk` and the `nextChunk`
    // otherwise `nextChunk` is allocated, and we can't merge these two
    if(chunk_two_hops_in_front->isPrevFree()) {
        // nextChunk is free so we need to merge that one too
        free_chunk->setSize(free_chunk->getSize() + next_chunk->getSize());
        removeFromFreeChunks(next_chunk);
        clearUpDataSpaceOfChunk(free_chunk);
        chunk_two_hops_in_front->setPrevFree();
        chunk_two_hops_in_front->setPrevSize(free_chunk->getSize());
    }
    // when the nextChunk is free, if chunkTwoHopsInFront was free, it would be merged in the step before.
    // This way we know that our chunk is free, and only one step around us can be free


    next_chunk = moveToTheNextChunk(free_chunk, free_chunk->getSize());


    if(next_chunk < af_arena_.top_) {
        // Set on the next that chunk before is free
        next_chunk->setPrevFree();
        next_chunk->setPrevSize(free_chunk->getSize());
    }else {
        // in this part of code we extend top to the free_chunk
        // this means we can't add free chunk to the free list
        next_chunk->setPrevFree();
        next_chunk->setPrevSize(free_chunk->getSize());
        // here we should actually merge our chunk with the top, and that way we have extended the unlimited free chunk
        extendTopChunk();
        return;
    }

    /// First we need to get the previous size of the chunk before us
    // wipe out the memory too

    // list of chunks how they became free in the list
    // last in first out
    if(af_arena_.unsorted_chunks_ == nullptr) {
        af_arena_.unsorted_chunks_ = free_chunk;
        af_arena_.unsorted_chunks_->setNext(free_chunk);
        af_arena_.unsorted_chunks_->setPrev(free_chunk);
        return;
    }
    Chunk *last_in_chunk = af_arena_.unsorted_chunks_;
    Chunk *first_in_chunk = last_in_chunk->getPrev();
    free_chunk->setNext(last_in_chunk);
    free_chunk->setPrev(first_in_chunk);

    last_in_chunk->setPrev(free_chunk);
    first_in_chunk->setNext(free_chunk);

    af_arena_.unsorted_chunks_ = free_chunk;

}

AfMalloc::~AfMalloc() {
    if(af_arena_.free_size_ != af_arena_.allocated_size_) {
        std::cout << "leaking memory" << std::endl;
    }
    munmap(af_arena_.begin_, af_arena_.allocated_size_);
}

std::optional<void*> AfMalloc::findChunkFromUnsortedFreeChunks(std::size_t needed_size) {
    // Next to the free chunk, unless it is in the fast bin range, there will always be an allocated chunk,
    // since otherwise we would coalesce them
    // on the free.

    // Unsorted free chunks are stored in a double linked list
    Chunk *start  = af_arena_.unsorted_chunks_;
    Chunk *free_chunk_iter = af_arena_.unsorted_chunks_;
    Chunk *match{nullptr};
    while(true) {
        // We are looking for the first chunk that we can find.
        // For others we will go through the list of the free chunks, and store them in the appropriate bin
        if(free_chunk_iter->getSize()  >= needed_size) {
            match = free_chunk_iter;
            break;
        }else {
            auto maybe_bin_index = findBinIndex(needed_size);
            // free_chunk_list -> 1 -> 2 - > 3
            auto *free_chunk_iter_prev = free_chunk_iter->getPrev();
            // We need to first unlink the chunk from the current place
            removeFromFreeChunks(free_chunk_iter);
            if(!maybe_bin_index) {
                moveToUnsortedLargeChunks(free_chunk_iter);
            }else {
                auto [index, bit_index] = *maybe_bin_index;
                if(index == FASTBINS_INDEX) {
                    // fast range
                    moveToFastBinsChunks(free_chunk_iter, bit_index);
                }else {
                    // small range
                    moveToSmallBinsChunks(free_chunk_iter, bit_index);
                }
            }

            // if we were the last chunk here
            if(free_chunk_iter_prev == free_chunk_iter) {
                break;
            }
        }

        free_chunk_iter = free_chunk_iter->getNext();

        if(free_chunk_iter == start) {
            break;
        }
    }

    if(match == nullptr) {
        return std::nullopt;
    }
    removeFromFreeChunks(match);
    // TODO this is opportunity to split the chunk on the multiple chunks, since we could otherwise get really
    // big chunk
    Chunk* next_chunk = moveToTheNextChunk(match, match->getSize());
    // next chunk only knows that we are free
    next_chunk->unsetPrevFree();
    // this will be used by our chunk also, so we need to zero the memory
    next_chunk->setPrevSize(0);

    // We don't need to call the construct here since we have already done so
    return moveToTheNextPlaceInMem(match, HEAD_OF_CHUNK_SIZE);
}

void *AfMalloc::malloc(std::size_t size) {

    std::size_t needed_size =  getMallocNeededSize(size);

    // if there are free chunks, try to use them
    if(af_arena_.unsorted_chunks_ != nullptr) {
        if(auto maybe_chunk = findChunkFromUnsortedFreeChunks(needed_size)) {
            return *maybe_chunk;
        }
    }
    if(isInFastBinRange()) {

    }
    if(isInSmallBinRange()) {

    }
    if(largeChunksAreFree()) {

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