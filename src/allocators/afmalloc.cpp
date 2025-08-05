#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <memory>
#include "afmalloc.hpp"

#include <sys/mman.h>

#define MMAP(addr, size, prot, flags) \
 mmap(addr, (size), (prot), (flags)|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)


constexpr std::size_t MAX_HEAP_SIZE = 32*4096;
constexpr bool TRACKING{false};


void AFMalloc::free(void *p) {
    auto *free_chunk = reinterpret_cast<Chunk *>(reinterpret_cast<uint64_t>(p) - sizeof(Chunk));
    free_chunk->m_size = 0;

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

    // fill out normal part of code
}

AFMalloc::~AFMalloc() {
    munmap(m_afarena.m_begin, m_afarena.m_allocated_size);
}

void do_allocation() {

}

std::size_t get_alignment_size(void* ptr, std::size_t alignment) {
    const auto int_ptr = reinterpret_cast<uintptr_t>(ptr);
    const auto aligned_ptr_int = (int_ptr - 1u + alignment) & -alignment;
    return  aligned_ptr_int - int_ptr;
}

void *AFMalloc::malloc(std::size_t size) {

    // if there is no enough free memory
    const std::size_t alignment_size = m_afarena.m_top != nullptr ? get_alignment_size(m_afarena.m_top, alignof(Chunk)):0;
    if(m_afarena.m_free_size < sizeof(Chunk) + size + alignment_size) {
        if(size + sizeof(Chunk)  + alignment_size > MAX_HEAP_SIZE) {
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
        std::cout << "Value is rounded up on page size: " << reinterpret_cast<uint64_t>(p1) << "" << reinterpret_cast<uint64_t>(p1)/4096 << std::endl;
        m_afarena.m_allocated_size = MAX_HEAP_SIZE;
        m_afarena.m_begin = p1;
        m_afarena.m_top= p1;
        m_afarena.m_free_size = MAX_HEAP_SIZE;
    }

    if(m_afarena.m_free_chunks != nullptr) {
        // deal with the case where we have free chunks
    }

    // let's say for simplistic reasons that for now alignment is 1 byte as we are working with chars and strings

    // this is actually wasting a lot of memory since m_prev and m_next are not used
    std::size_t total_needed_size = size + sizeof(Chunk);
    // what we will do is we will return to user pointer after chunk's block

    // we need to take care of alignment needed for chunk

    // align updates m_free_size and m_top if alignment succeeds
    // In this case what we will do is waste couple of bytes for alignment purposes of the begining of the chunk,
    // create chunk in place and give the rest of the needed memory to the user
    // for chunk alignment we need 8 bytes, chunk size is 24 bytes
    // memory:  0 1 2 3 4 5 6 7 8 9 0 1 2 ... 0 1 2 3
    //       :  [chunk               .....          ]
    //
    // I noticed now that if user requests 13 bytes that is the amount of bytes we will give him
    // Also that we might not have aligned memory on our side in that case and we might have a problem connecting two blocks

    auto *user_ptr = std::align(alignof(Chunk), total_needed_size, m_afarena.m_top, m_afarena.m_free_size);
    if(user_ptr == nullptr){
        // we need to extend arena
    }

    // std::align moves m_top when we aligned it for the top
    // TODO(afilipovic) enable std::construct
    // we don't fill here prev and next as they are not used when chunk is in allocated state
    ::new (user_ptr) Chunk{total_needed_size, nullptr, nullptr}; // constructs chunk in place
    // the size for alignment of a Chunk is already reduced from m_afarena.m_free_size
    m_afarena.m_free_size -=  total_needed_size;
    m_afarena.m_top = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(user_ptr) + total_needed_size);

    return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(user_ptr)+sizeof(Chunk));
}

void *AFMalloc::memAlign([[maybe_unused]] std::size_t alignment, [[maybe_unused]] std::size_t size) {
    return nullptr;
}