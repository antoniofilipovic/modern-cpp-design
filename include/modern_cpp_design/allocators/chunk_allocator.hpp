#include <cstdint>
#include <page_size_allocator.hpp>
#include <vector>
#include <cassert>
#include "memory_pool_allocator.hpp"


namespace memory{
    class ChunkAllocator{


    public:
        explicit ChunkAllocator(MemoryPoolAllocator &memory_pool_allocator): memory_pool_allocator_(memory_pool_allocator){}

        // what about copy ctor, move ctor, etc



        void *allocate(std::size_t alignment, std::size_t size){
            // 1. check if you have chunk which is free
            // 2. if not call memory pool allocate
            // 3. write on top of the chunk data about it being top/intern part of chunk
            // 4. call align and return alignment + size
            // 5. move chunks pointer to the new free spot
            // 6. wait for new allocation


            // other idea is for each other chunk to have pointer to top level chunk
            // top level chunk contains only counter on number of allocations


            memory_pool_allocator_;
        }

        void deallocate(void *ptr){
            // check if it is top level chunk
            // if not mark that region as deallocated
        }
        void release() {
            /// todo think about fragmentation
        }

    private:
        struct TopLevelPointer{
            uint64_t *ptr_;
            bool is_top_level_;
        };


        struct Chunk{
            uint64_t *start_ptr_;
            uint64_t *current_ptr_;
            std::size_t free_bytes_;
            std::size_t num_allocations_;
        };

        static_assert(alignof(TopLevelPointer) == 8);
        static_assert(sizeof(TopLevelPointer) == 16);

        MemoryPoolAllocator &memory_pool_allocator_;
        std::vector<Chunk> chunks_;
        void *active_chunks_{nullptr};


    };


    /// chunk_start
    //  bytes       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
    //              |........TopLevelPointer......| | int |alignment|........TopLevelPointer......| | int |



}