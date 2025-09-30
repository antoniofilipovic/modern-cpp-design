#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <memory>
#include <cstring>
#include "afmalloc.hpp"

#include <sys/mman.h>

#define MMAP(addr, size, prot, flags) \
 mmap(addr, (size), (prot), (flags)|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)


constexpr std::size_t MAX_HEAP_SIZE = 32*4096;
constexpr bool TRACKING{false};
constexpr std::size_t SIZE_OF_SIZE = sizeof(std::size_t);
static_assert(SIZE_OF_SIZE == 8);
constexpr std::size_t ALIGNMENT = 2 * SIZE_OF_SIZE;
constexpr std::size_t ALIGNMENT_MASK = ALIGNMENT - 1;
constexpr std::size_t CHUNK_SIZE = sizeof(Chunk);
constexpr std::size_t HEAD_OF_CHUNK = offsetof(Chunk, m_prev);
static_assert(HEAD_OF_CHUNK == 16);


Chunk *moveToThePreviousChunk(void *ptr, std::size_t size) {
    return reinterpret_cast<Chunk *>(reinterpret_cast<uintptr_t>(ptr) - size);;
}

Chunk *moveToTheNextChunk(void *ptr, std::size_t size) {
    return reinterpret_cast<Chunk *>(reinterpret_cast<uintptr_t>(ptr) - size);;
}

void* moveToTheNextPlaceInMem(void *ptr, std::size_t size) {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + size);
}

void* moveToThePreviousPlaceInMem(void *ptr, std::size_t size) {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) - size);
}

std::size_t getSize(Chunk *chunk) {
    return chunk->m_size & ~PREV_FREE;
}

// Once deallocation is done, we write to the next chunk that prev_size is our size current, and we write to
// ourselves that we are not in use
// That should enable merging of two chunks
void AFMalloc::free(void *p) {
    auto *free_chunk = reinterpret_cast<Chunk *>(reinterpret_cast<uint64_t>(p) - HEAD_OF_CHUNK);
    //free_chunk->m_size;
    //free_chunk->inUse=false;

    /// here
    /// [Already freed chunk][Current free chunk][Free or allocated chunk]
    ///
    // This is where IN_USE bit comes into play
    // []

    // linking chunks in the list of free chunks is good but let's skip it now

    // Here we want to check if the chunk in the physical memory before us has actually
    // been freed. If so, we can try to merge those two
    if(free_chunk->m_size & PREV_FREE) {
        // previous chunk is free, we need to merge them
        std::size_t prev_size = free_chunk->m_previous_size;

        /// Then we need to go to that place in the memory
        auto *chunk_before = moveToThePreviousChunk(free_chunk, prev_size);
        chunk_before->m_size = prev_size + free_chunk->m_size;

        free_chunk = chunk_before;

        auto *data_start =  moveToTheNextPlaceInMem(free_chunk, HEAD_OF_CHUNK); // move to the place where data starts
        memset(data_start, 0, chunk_before->m_size);
    }

    auto *nextChunk = moveToTheNextChunk(free_chunk, free_chunk->m_size);

    // we are reaching at boundary, we can't do anything here
    if (nextChunk >= m_afarena.m_top) {
        // here we should actually merge our chunk with the top, and that way we have extended the unlimited free chunk
        return;
    }
    Chunk *chunkTwoHopsInfront =  moveToTheNextChunk(nextChunk, nextChunk->m_size);

    if(static_cast<void*>(chunkTwoHopsInfront) >= m_afarena.m_top) {
        // we are at the boundary. This means that nextChunk is allocated. Because if it would be free
        // we would extend m_top
        // Here we need to write that we are free
        nextChunk->m_size |= PREV_FREE;
    }

    if(chunkTwoHopsInfront->m_size & PREV_FREE) {
        // nextChunk is free so we need to merge that one too
        free_chunk->m_size = free_chunk->m_size + nextChunk->m_size;
        chunkTwoHopsInfront->m_size |= PREV_FREE;
    }else {
        // nextChunk is allocated, but we need to write to it that we are free
        nextChunk->m_size |= PREV_FREE;
    }

    ///// c0:free_C1:allocated(p)_c2:free|allocated
    /// First we need to get the previous size of the chunk before us
    // wipe out the memory too

    // list of chunks how they became free in the list
    // last in first out
    if(m_afarena.m_free_chunks == nullptr) {
        m_afarena.m_free_chunks = free_chunk;
        m_afarena.m_free_chunks->m_next = free_chunk;
        m_afarena.m_free_chunks->m_prev = free_chunk;
        return;
    }
    Chunk *last_in_chunk = m_afarena.m_free_chunks;
    Chunk *first_in_chunk = last_in_chunk->m_prev;
    free_chunk->m_next = last_in_chunk;
    free_chunk->m_prev = first_in_chunk;

    last_in_chunk->m_prev = free_chunk;
    first_in_chunk->m_next = free_chunk;

    m_afarena.m_free_chunks = free_chunk;

}

AFMalloc::~AFMalloc() {
    munmap(m_afarena.m_begin, m_afarena.m_allocated_size);
}

void do_allocation() {

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
    return (size + SIZE_OF_SIZE + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;

}
void *AFMalloc::malloc(std::size_t size) {
    // We need to think that
    // 1. We allocate 8bytes at least for user
    // 2. Those 8 bytes are aligned on the std::size_t so we can write down the size


    // if there is no enough free memory
    const std::size_t alignment_size = m_afarena.m_top != nullptr ? get_alignment_size(m_afarena.m_top, alignof(Chunk)): 0;
    // assert here that m_afarena.m_top is always on the 16 byte boundary
    std::size_t needed_size =  getMallocNeededSize(size);

    // if there are free chunks, try to use them
    if(m_afarena.m_free_chunks != nullptr) {
        // deal with the case where we have free chunks
    }

    // if there are no free chunks, and we have no enough size, we need to allocate a new block
    if(m_afarena.m_free_size < needed_size) {
        if(needed_size > MAX_HEAP_SIZE) {
            assert(false); // unsupported case
        }
        if(m_afarena.m_top != nullptr) {
            assert(false); // unsupported case with missing new heap
        }
        // allocate block of memory, I am not sure if this block is aligned on anything other than page size
        void *p1 = MMAP (nullptr, MAX_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE);
        if(p1 == nullptr) {
            return nullptr;
        }
        assert(get_alignment_size(p1, 4096) == 0); // page size is 4096 bytes or 4kB
        std::cout << "Value is rounded up on page size: " << reinterpret_cast<uint64_t>(p1) << "" << reinterpret_cast<uint64_t>(p1)/4096 << std::endl;
        m_afarena.m_allocated_size = MAX_HEAP_SIZE;
        m_afarena.m_begin = p1;
        m_afarena.m_top= p1;
        m_afarena.m_free_size = MAX_HEAP_SIZE;
    }

    // Here we can store anything which has alignment to 16 bytes. All the objects needed have number which is even
    // for allocation reasons, hence we can be sure here that everything will properly work.

    // Here we will give to user the size needed
    // what we will do is we will return to user pointer after chunk's block

    // We need to take care of alignment needed for chunk

    // std::align moves m_top when we aligned it for the top
    // TODO(afilipovic) enable std::construct
    // we don't fill here prev and next as they are not used when chunk is in allocated state
    // constructs chunk in place

    auto *user_ptr = m_afarena.m_top;
    ::new (user_ptr) Chunk{0, needed_size, nullptr, nullptr};

    m_afarena.m_free_size -=  needed_size;
    m_afarena.m_top = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(user_ptr) + needed_size);

    return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(user_ptr)+HEAD_OF_CHUNK);
}

void *AFMalloc::memAlign([[maybe_unused]] std::size_t alignment, [[maybe_unused]] std::size_t size) {
    return nullptr;
}