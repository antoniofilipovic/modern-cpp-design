#pragma once
#include <cstdint>

class AFMalloc{

    // struct which holds arena


    public:
      explicit AFMalloc(bool use_sbrk, std::size_t requested_initial_size);


      void *malloc(std::size_t size);

    void *memAlign(std::size_t alignment, std::size_t size);

    void free(void *p);

      ~AFMalloc();

    private :

        // 1. How to know if chunk is in use or not
        // 2. chunk when it is allocated it will have size set and
        //  m_prev and m_next will be nullptr
        struct Chunk{
              std::size_t m_size{0}; // this is chunk total size
              Chunk *m_prev{nullptr};
              Chunk *m_next{nullptr};
        };

        // 2.
        struct AFArena{

          explicit AFArena() : m_begin{nullptr} {}

          // begin of arena
          void *m_begin{nullptr};

          // beginning of the rest of the memory region
            // here the free region starts
          void *m_top{nullptr};

            // this is total allocated size
          std::size_t m_allocated_size{0};

          // this is leftover size
          std::size_t m_free_size{0};

          // pointer to the linked list of free chunks
          Chunk *m_free_chunks{};

        };




    AFArena m_afarena{};
    bool m_use_sbrk{false};

};