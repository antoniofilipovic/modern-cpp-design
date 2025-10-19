#pragma once
#include <cstdint>
#include <bit>
#include <bitset>
#include <vector>

// original malloc implementation has fastBins from 32 to 160 bytes
// fast bins are 16 bytes apart

// small bin range is to 512 bytes ?
//

// when there is a large request it tries to consolidate first fast bins as it helps with fragmentation
// fastchunks are not consolidated otherwise

static_assert(std::endian::native == std::endian::little);

static_assert(sizeof(std::size_t) == 8);

static std::size_t PREV_FREE = 1ul << 63;

// 32 pages, or 128kB
constexpr std::size_t MAX_HEAP_SIZE = 32*4096;
constexpr bool TRACKING{false};
constexpr std::size_t SIZE_OF_SIZE = sizeof(std::size_t);
static_assert(SIZE_OF_SIZE == 8);
constexpr std::size_t ALIGNMENT = 2 * SIZE_OF_SIZE;
constexpr std::size_t ALIGNMENT_MASK = ALIGNMENT - 1;
static std::size_t HEAD_OF_CHUNK_SIZE = 16;

/**
 *
 */
class Chunk{
  public:

    Chunk(const std::size_t prev_size, const std::size_t size, Chunk *prev, Chunk *next): previous_size_(prev_size), size_(size), prev_(prev), next_(next) {}

    [[nodiscard]] std::size_t getSize() const {
      return size_ & ~PREV_FREE;
    }

    void setSize(const std::size_t size) {
      size_ = (size & ~PREV_FREE);
    }

    [[nodiscard]] bool isPrevFree() const {
      return size_ & PREV_FREE;
    }

    void setPrevFree() {
      size_ |= PREV_FREE;
    }

    void unsetPrevFree() {
      size_ &= ~PREV_FREE;
    }

    [[nodiscard]] std::size_t getPrevSize() const {
      return previous_size_;
    }

    void setPrevSize(const std::size_t previous_size) {
      previous_size_ = previous_size;
    }

    [[nodiscard]] Chunk * getPrev() {
      return prev_;
    }

    [[nodiscard]] Chunk* getNext() {
      return next_;
    }

    void setPrev(Chunk *prev) {
      prev_ = prev;
    }

    void setNext(Chunk *next) {
      next_ = next;
    }



  private:
    std::size_t previous_size_{0};
    std::size_t size_{0}; // this is chunk total size
    Chunk *prev_{nullptr};
    Chunk *next_{nullptr};

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


static_assert(sizeof(Chunk) == 32, "Expected size of chunk to be 32");
static_assert(alignof(Chunk) == 8);
constexpr std::size_t CHUNK_SIZE = sizeof(Chunk);


Chunk *moveToThePreviousChunk(void *ptr, std::size_t size);

Chunk *moveToTheNextChunk(void *ptr, std::size_t size);

void* moveToTheNextPlaceInMem(void *ptr, std::size_t size);

void* moveToThePreviousPlaceInMem(void *ptr, std::size_t size);

/**
 *
 * @param size size for which to calculate how much we need to allocate
 * @return number of bytes needed
 */
std::size_t getMallocNeededSize(std::size_t size);

/**
 * Basic struct with arena. It contains free_size_ of the top chunk,
 * pointer to the top_ chunk, pointer to the begining of the memory block
 * totally allocated size.
 *
 * Other structs include pointer to unsorted chunks, which in the first case when chunk is freed it is put
 * to that list of the unsorted_chunks, which speeds up free, and gives them the chance to be reused quickly.
 *
 * For the unsorted_chunks_ I am doing LIFO, whereas malloc does FIFO
 */
struct AfArena{

  explicit AfArena() = default;

  // begin of arena
  void *begin_{nullptr};

  // beginning of the rest of the memory region
  // here the free region starts
  void *top_{nullptr};

  // this is total allocated size
  std::size_t allocated_size_{0};

  /**
   * Tracks the number of free bytes left in the top chunk
   */
  std::size_t free_size_{0};

  /**
   * Contains the list of unsorted chunks. List is populated on the free, and then on the malloc
   * we put the chunk in the corresponding list.
   */
  Chunk *unsorted_chunks_{};

  /**
   * Index list to help find if there are some free chunks there or not
   */
  std::vector<std::bitset<32>> bin_indexes_{};

  /**
   * Chunks which are not consolidated
   */
  Chunk *fast_chunks_{};

  /**
   *
   */
  Chunk *small_chunks_{};


};

class AfMalloc{

  // struct which holds arena
  public:
    explicit AfMalloc() = default;

    /**
     * Main malloc function used for satisfying user requests
     * @param size size user needs
     * @return the pointer to the memory object
    */
    void *malloc(std::size_t size);

  /**
   * Allocate with the user requested alignment, of at least size bytes
   * @param alignment
   * @param size
   * @return
   */
  void *memAlign(std::size_t alignment, std::size_t size);

    /**
      *
      * @param p chunk
    */
    void free(void *p);

    [[nodiscard]] std::size_t getFreeSize() const {
      return af_arena_.free_size_;
    }

    [[nodiscard]] std::size_t getAllocatedSize() const {
      return af_arena_.allocated_size_;
    }

    [[nodiscard]]  void * getTop() const {
      return af_arena_.top_;
    }

    [[nodiscard]] void *getBegin() const {
      return af_arena_.begin_;
    }

    /**
      *
      * @return
    */
    Chunk *getUnsortedChunks() {
      return af_arena_.unsorted_chunks_;
    }

    ~AfMalloc();


    void printArenasMemory() {}

    void extendTopChunk();

  private:
      // removes this chunk from the list of free chunks
      void removeFromFreeChunks(Chunk* chunk);
      AfArena af_arena_{};
};