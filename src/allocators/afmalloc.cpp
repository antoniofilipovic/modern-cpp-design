#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <memory>
#include "afmalloc.hpp"

#include <sys/mman.h>

#define MMAP(addr, size, prot, flags) \
 mmap(addr, (size), (prot), (flags)|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)


static constexpr std::size_t MAX_HEAP_SIZE = 32*4096;
AFMalloc::AFMalloc(bool use_sbrk, std::size_t requested_initial_size): m_use_sbrk(use_sbrk) {
    if(m_use_sbrk) {
        void *current_break = sbrk(0);
        m_afarena.m_begin = current_break;
        std::cout << "Current break" << reinterpret_cast<uint64_t>(m_afarena.m_begin) << std::endl;
        std::cout << requested_initial_size << std::endl;
    }else {
        m_afarena.m_begin = nullptr;
    }

}

void AFMalloc::free(void *p) {
    auto *free_chunk = reinterpret_cast<Chunk *>(reinterpret_cast<uint64_t>(p) - sizeof(Chunk));
    free_chunk->m_size = 0;

    if(m_afarena.m_free_chunks == nullptr) {
        m_afarena.m_free_chunks = free_chunk;
        return;
    }
    /// next_free_chunk -> current_free_chunk->next -> nullptr| chunk
    Chunk *next_free_chunk = m_afarena.m_free_chunks;
    if(next_free_chunk->m_next == nullptr) {
        assert(next_free_chunk->m_prev == nullptr);

        // it should be double linked list where end points to the beginning
        next_free_chunk->m_next = free_chunk;
        next_free_chunk->m_prev = free_chunk;
        free_chunk->m_prev = next_free_chunk;
        free_chunk->m_next = next_free_chunk;
        return;
    }

    // fill out normal part of code
}

AFMalloc::~AFMalloc() {
    if(m_use_sbrk) {
        // is this enough to deallocate?
        sbrk(-static_cast<int64_t>(m_afarena.m_allocated_size));
    }else {
        munmap(m_afarena.m_begin, m_afarena.m_allocated_size);
    }

}

void do_allocation() {

}

void *AFMalloc::malloc(std::size_t size) {

    //assert((sbrk(0) == m_afarena.m_begin) && "sbrk(0) gives different address than on the beginning");
    // initialization of arena
    if(m_afarena.m_allocated_size == 0) {
        if(!m_use_sbrk) {
            // allocate block of memory, I am not sure if this block is aligned on anything other than page size
            void *p1 = MMAP (nullptr, MAX_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE);
            if(p1 == nullptr) {
                return nullptr;
            }
            //
            // if (mprotect (p1, size, PROT_READ | PROT_WRITE) != 0)
            // {
            //     munmap (p1, MAX_HEAP_SIZE);
            //     return nullptr;
            // }
            std::cout << "Value is rounded up on page size: " << reinterpret_cast<uint64_t>(p1) << "" << reinterpret_cast<uint64_t>(p1)/4096 << std::endl;
            m_afarena.m_allocated_size = MAX_HEAP_SIZE;
            m_afarena.m_begin = p1;
            m_afarena.m_top= p1;
            m_afarena.m_free_size = MAX_HEAP_SIZE;

        }else {
            void *ptr = sbrk(MAX_HEAP_SIZE);
            if(ptr == reinterpret_cast<void *>(-1)) {
                std::cout << "sbrk does not work" << std::endl;
            }
            assert(ptr == m_afarena.m_begin);
            m_afarena.m_allocated_size += 32*4096; // 128 kb, same as what malloc does
            m_afarena.m_free_size = m_afarena.m_allocated_size;
            m_afarena.m_top = m_afarena.m_begin;
        }

    }

    // let's say for simplistic reasons that for now alignment is 1 byte as we are working with chars and strings

    // we need size that user requested and additional size of chunk
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
    assert(user_ptr == m_afarena.m_top);
    ::new (user_ptr) Chunk{total_needed_size, nullptr, nullptr}; // constructs chunk in place
    m_afarena.m_free_size -=  total_needed_size;
    m_afarena.m_top = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(user_ptr) + total_needed_size);

    return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(user_ptr)+sizeof(Chunk));
}

void *AFMalloc::memAlign([[maybe_unused]] std::size_t alignment, [[maybe_unused]] std::size_t size) {
    return nullptr;
}