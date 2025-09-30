#pragma once
#include <cstdint>
#include <bit>
static_assert(std::endian::native == std::endian::little);

struct Chunk{
  std::size_t m_previous_size{0};
  std::size_t m_size{0}; // this is chunk total size
  Chunk *m_prev{nullptr};
  Chunk *m_next{nullptr};
};

// When the chunk is allocated we store the user object from m_prev forwards

// allocated chunk looks like this

/// --  prev_size <- this is set only if the previous chunk is free
/// --  size <- size of the current chunk
/// -- user data
/// -- user data
///   [.....]
///
///
///  free chunk looks like this
///  prev_size is set if the previous chunk is free. In our algorithm this should not happen as we coalesce two free chunks
///  size
///  m_prev
///  m_next ptr


static_assert(sizeof(std::size_t) == 8);
static_assert(sizeof(Chunk) == 32, "Expected size of chunk to be 32");
static_assert(alignof(Chunk) == 8);
static std::size_t PREV_FREE = 1ul << 63;


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
      AFArena m_afarena{};
};