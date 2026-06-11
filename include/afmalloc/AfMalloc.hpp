#pragma once
#include <atomic>
#include <cstdint>
#include <bit>
#include <bitset>
#include <format>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>


struct GlobalConfig {
  bool isTrackingGlobal{false};
  bool disableRoundRobin{false};
};


inline GlobalConfig& getGlobalConfig() {
  static GlobalConfig globalConfig{};
  return globalConfig;
}



// original malloc implementation has fastBins from 32 to 160 bytes
// fast bins are 16 bytes apart

// small bin range is to 512 bytes ?
//

// when there is a large request it tries to consolidate first fast bins as it helps with fragmentation
// fastchunks are not consolidated otherwise

static_assert(std::endian::native == std::endian::little);


const std::size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);
constexpr std::size_t SIZE_OF_SIZE = sizeof(std::size_t);
static_assert(SIZE_OF_SIZE == 8);

// How we mark that previous is inuse
// 63 may come out of random, but it is sizeof(std::size_t) * CHAR_NUM_BITS - 1
static std::size_t PREV_FREE = 1ul << 63;
static std::size_t EMPTY_FLAG = 0ul;

// 32 pages, or 128kB
const std::size_t MAX_HEAP_SIZE = 32*PAGE_SIZE;

constexpr std::size_t ALIGNMENT = 2 * SIZE_OF_SIZE;
constexpr std::size_t ALIGNMENT_MASK = ALIGNMENT - 1;


static std::size_t HEAD_OF_CHUNK_SIZE = 16;
static std::size_t TOP_CHUNK_NEEDED_SIZE = 16;

constexpr std::size_t FAST_BIN_RANGE_START = 16;
constexpr std::size_t FAST_BIN_RANGE_END = 160;
constexpr std::size_t NUM_FAST_CHUNKS = FAST_BIN_RANGE_END / FAST_BIN_RANGE_START;

constexpr std::size_t SMALL_BIN_RANGE_END = 512;

constexpr std::size_t BIN_SPACING_SIZE = 16;
constexpr std::size_t BITMAP_SIZE = 32;
constexpr std::size_t FASTBINS_INDEX = 0;
constexpr std::size_t SMALLBINS_INDEX = 1;

constexpr std::size_t NUM_SMALL_CHUNKS = (SMALL_BIN_RANGE_END - FAST_BIN_RANGE_END) / BIN_SPACING_SIZE;

static_assert((SMALL_BIN_RANGE_END - FAST_BIN_RANGE_END) / BIN_SPACING_SIZE <= BITMAP_SIZE);

constexpr std::size_t NUM_ARENAS_FACTOR = 1;
constexpr std::size_t NUM_CORES = 2; // find the function which can provide correct num cores

class OnScopeExit {
  public:
    explicit OnScopeExit (std::function<void()> func) : function_(std::move(func)) {

    }

    ~OnScopeExit() {
      std::invoke(function_);
    }

  private:
    std::function<void()> function_;

};

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

    [[nodiscard]] const Chunk * getPrev()  const {
      return prev_;
    }

    [[nodiscard]] const Chunk* getNext() const {
      return next_;
    }

    void setPrev(Chunk *prev) {
      prev_ = prev;
    }

    void setNext(Chunk *next) {
      next_ = next;
    }

    [[nodiscard]] std::size_t getFlags() const {
      return isPrevFree()? PREV_FREE : 0;
    }

    bool operator==(const Chunk &other) const {
      return other.getNext() == getNext() && other.getPrev() == getPrev() &&  other.size_== size_ && other.previous_size_ == previous_size_;
    }



  private:
    std::size_t previous_size_{0};
    std::size_t size_{0}; // this is chunk total size
    Chunk *prev_{nullptr};
    Chunk *next_{nullptr};
};

template <>
struct std::hash<Chunk*> {
  std::size_t operator()(const Chunk *chunk) const noexcept {
    return std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(chunk));
  }

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


bool isChunkCoalescable(const Chunk &chunk);

std::size_t getPtrDiffSize(void *second, void *first);

/**
 *
 * @param size size for which to calculate how much we need to allocate
 * @return number of bytes needed
 */
std::size_t getMallocNeededSize(std::size_t size);


std::optional<std::pair<std::size_t, std::size_t>> findBinIndex(std::size_t allocations_size);

class AfArena;


/**
 * Heap is linking to the previous heap.
 * And arena is pointing to the last allocated heap
 */
struct AfHeap {
  std::size_t heap_size_{0};
  AfArena *arena_ptr_;
  AfHeap *prev_heap_{};

};
static_assert(sizeof(AfHeap) == 24, "Expected size of AfHeap to be 24");
static_assert(alignof(AfHeap) == 8, "Expected size of AfHeap to be 8");



/**
 * Returns how many bytes are we misaligned
 * @param ptr
 * @param alignment
 * @return
 */
inline std::size_t getAlignmentSize(void* ptr, std::size_t alignment) {
  const auto int_ptr = reinterpret_cast<uintptr_t>(ptr);
  const auto aligned_ptr_int = (int_ptr + (alignment - 1u)) & -alignment;
  return  aligned_ptr_int - int_ptr;
  /// -alignment what is does is it creates a number which is  000010000  ->  111011111 + 1 -> 11100000  111111010000
  /// if ptr is aligned on something other than 16, when you add 15 it will move it to the next alignment slot
}

inline AfHeap *getHeapAddress(void *p) {
  // We guarantee that heap address is allocated on 32 page boundary
  // Hence here we divide
  auto missAlignment = getAlignmentSize(p, MAX_HEAP_SIZE);
  auto base = reinterpret_cast<uintptr_t>(p) - (MAX_HEAP_SIZE - missAlignment);
  //
  assert((reinterpret_cast<uintptr_t>(p) & (~MAX_HEAP_SIZE + 1)) == base);
  return std::bit_cast<AfHeap *>(base);
}


/**
 * Basic struct with arena. It contains free_size_ of the top chunk,
 * pointer to the top_ chunk, pointer to the begining of the memory block
 * totally allocated size.
 *
 * Other structs include pointer to unsorted chunks, which in the first case when chunk is freed it is put
 * to that list of the unsorted_chunks, which speeds up free, and gives them the chance to be reused quickly.
 *
 * For the unsorted_chunks_ I am doing LIFO, whereas malloc does FIFO.
 * FIFO should give the equal opportunity to each chunk to be reused, and then consolidated, thus reducing
 * fragmentation. LIFO is done only since I don't care here about fragmentation and such long lived programs.
 */
class AfArena{

public:


  AfArena();



  //AfArena(const AfArena &) = delete;

  //AfArena(AfArena &&other) = delete;

  void initializeAfArena();

  std::mutex arena_lock_{};

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
   * we put the chunk in the corresponding bin. We traverse this double linked list in LIFO order, which is
   * different from what malloc does (FIFO). FIFO is better for giving chunks a chance to be reused.
   */
  Chunk unsorted_chunks_{0, 0, nullptr, nullptr};

  /**
   * Index list to help find if there are some free chunks there or not
   */
  std::vector<std::bitset<32>> bin_indexes_{};

  /**
   * Pointer to the beginning of every of the fast chunks
   */
  std::vector<Chunk> fast_chunks_{};

  /**
   * Pointer to the beginning of the every of the small chunks
   */
  std::vector<Chunk> small_chunks_{};

  /**
   * Large chunks which are unsorted
   */
  Chunk unsorted_large_chunks_{0, 0, nullptr, nullptr};


  AfHeap *last_heap_{nullptr};

  // AfArena *next_arena_;
  //
  // AfArena *prev_arena_;

  [[nodiscard]] std::size_t getFreeSize() const {
    return free_size_;
  }


  [[nodiscard]]  void * getTop() const {
    return top_;
  }


  void extendTopChunk();


  void* findChunkFromUnsortedFreeChunks(std::size_t size);

  void moveChunkToCorrectBin(Chunk *current_chunk, std::size_t size);

  void moveToUnsortedLargeChunks(Chunk *free_chunk);

  void moveToFastBinsChunks(Chunk *free_chunk, std::size_t bit_index);

  void moveToSmallBinsChunks(Chunk *free_chunk, std::size_t bit_index);

  Chunk *tryFindFastBinChunk(std::size_t size);

  Chunk *tryFindSmallBinChunk(std::size_t size);


  void setBinIndex(std::size_t bin, std::size_t bit);

  void unsetBitIndex(std::size_t bin, std::size_t bit);

  bool isBinBitIndexSet(std::size_t bin, std::size_t bit);


};

// Strong type for Chunk*
struct ListHead {
  //explicit ListHead(Chunk *list_head) : list_head_(list_head) {}
  Chunk *list_head_;
};

struct ListHeadRef {
  //explicit ListHead(Chunk *list_head) : list_head_(list_head) {}
  std::reference_wrapper<Chunk*> list_head_;
};


bool isInFastBinRange(std::size_t size);

bool isInSmallBinRange(std::size_t size);

bool hasLargeChunkFree();



Chunk *tryFindLargeChunk(Chunk *large_chunks, std::size_t size);

/////// Utility methods

bool isPointingToSelf(const Chunk &list_head);

bool hasElementsInList(const Chunk &list_head);

void unlinkChunk(Chunk* chunk);




class AfMalloc{

  // struct which holds arena
  public:
    explicit AfMalloc();

    explicit AfMalloc(bool track_pointers) ;


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

    // TODO move this methods out and fix const


    [[nodiscard]] std::size_t getAllocatedSize(const AfArena &af_arena) const {
      return af_arena.allocated_size_;
    }



    Chunk *getUnsortedChunks(AfArena &af_arena) {
      return &af_arena.unsorted_chunks_;
    }

    std::vector<Chunk> &getFastBinChunks(AfArena &af_arena) {
      return af_arena.fast_chunks_;
    }

    std::vector<Chunk> &getSmallBinChunks(AfArena &af_arena) {
      return af_arena.small_chunks_;
    }

    void dumpMemory();

    std::string getPtrHumaneReadableName(Chunk *chunk) {
      auto iter = name_map_.find(chunk);
      assert(iter != name_map_.end());
      return iter->second;
    }

    std::string createPtrHumaneReadableName(std::string_view prefix, Chunk *chunk) {
      auto iter = name_map_.find(chunk);
      if(iter != name_map_.end()) {
        return iter->second;
      }
      name_counter_[std::string{prefix}];

      auto [insert_iter, ok ] = name_map_.emplace(chunk, std::format("{}_{}", prefix, name_counter_[std::string{prefix}]++));
      return insert_iter->second;
    }

    ~AfMalloc();


    void printArenasMemory() {}





  private:
      void init();


      // removes this chunk from the list of free chunks


    /**
     * Method allocateNewHeap does the following
     *  1. In case there is no heap at all allocated, it creates a new heap and links it to arena
     *  2. In case there is old heap which does not have enough memory for new allocation, it creates a new heap
     *  and links the old heap to the new one
     *  3. TODO From the old heap it chunks the data and moves it to the freeChunk list
     *
     *
     * @param arena
     * @return
     */
      bool allocateNewHeap(AfArena &arena);

      AfArena* getArena();


      std::vector<std::unique_ptr<AfArena>> arenas_{};
      // https://www.reddit.com/r/ProgrammerTIL/comments/4tspsn/c_it_will_take_approximately_97_years_to_overflow/
      // if my program is running and doing allocation every 1 cpu cycle it will overflow in 97 years
      std::atomic<uint64_t> next_arena_{0};

      std::size_t max_num_arenas_{NUM_ARENAS_FACTOR * NUM_CORES};

      std::unordered_map<Chunk *, std::string> name_map_{};
      std::unordered_map<std::string, std::size_t> name_counter_{};
      bool track_pointers_{false};

};




template<typename... Args>
std::string dyna_print(std::string_view rt_fmt_str, Args&&... args)
{
  return std::vformat(rt_fmt_str, std::make_format_args(args...));
}
