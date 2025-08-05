#pragma once
#include <cstdint>

struct Chunk{
  std::size_t m_size{0}; // this is chunk total size
  Chunk *m_prev{nullptr};
  Chunk *m_next{nullptr};
};
static_assert(sizeof(Chunk) == 24, "Expected size of chunk to be 24");

// 2.
struct AFArena{

  explicit AFArena() = default;

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
class AFMalloc{

    // struct which holds arena

    public:
      explicit AFMalloc() = default;


    void *malloc(std::size_t size);

    void *memAlign(std::size_t alignment, std::size_t size);

    void free(void *p);

    [[nodiscard]] std::size_t getFreeSize() const {
      return m_afarena.m_free_size;
    }

   [[nodiscard]] std::size_t getAllocatedSize() const {
      return m_afarena.m_allocated_size;
   }

   [[nodiscard]]  void const * getTop() const {
      return m_afarena.m_top;
    }

   [[nodiscard]] void const *getBegin() const {
      return m_afarena.m_begin;
    }

    ~AFMalloc();

    private :
        // 1. How to know if chunk is in use or not
        // 2. chunk when it is allocated it will have size set and
        //  m_prev and m_next will be nullptr

    AFArena m_afarena{};
};