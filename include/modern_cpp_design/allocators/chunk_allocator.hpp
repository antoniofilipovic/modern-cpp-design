#include <cstdint>
#include <page_size_allocator.hpp>
#include <vector>
#include <cassert>
#include <functional>
#include <memory>

#include "memory_pool_allocator.hpp"


namespace memory{
    constexpr static std::size_t MAX_CHUNK_SIZE = 512;
    class ChunkAllocator{

    public:
        explicit ChunkAllocator(MemoryPoolAllocator &memory_pool_allocator): memory_pool_allocator_(memory_pool_allocator){}

        // what about copy ctor, move ctor, etc

        void *allocate(std::size_t alignment, std::size_t size){
            // other idea is for each other chunk to have pointer to top level chunk
            // top level chunk contains only counter on number of allocations
            assert(size <= MAX_CHUNK_SIZE);

            auto const allocate_new_chunk = [this, &alignment]() {
                // always allocate max size for chunk
                void *p = memory_pool_allocator_.allocate(alignment, MAX_CHUNK_SIZE);
                assert(p!=nullptr);
                chunks_.emplace_back(p, p, MAX_CHUNK_SIZE, 0);
            };



            if(chunks_.empty()) {
               std::invoke(allocate_new_chunk);
            }

            for(auto &chunk : chunks_) {
                if(chunk.free_bytes_ >= size) {
                    // can we align the pointer
                    void *maybe_enough = chunk.current_ptr_;
                    std::size_t free_size = chunk.free_bytes_;
                    // align checks not just if it has enough space for alignment but also if there is
                    // enough space to add the element inside
                    void *maybe_aligned_ptr = std::align(alignment, size, maybe_enough, free_size);
                    if(maybe_aligned_ptr != nullptr) {
                        chunk.free_bytes_ = free_size - size;
                        chunk.current_ptr_ = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(maybe_aligned_ptr)+size);
                        chunk.num_allocations_+=1;
                        return maybe_aligned_ptr;
                    }
                }
            }

            // try allocate again the free chunk
            //

            std::invoke(allocate_new_chunk);
            auto &chunk = chunks_.back();

            void *maybe_enough = chunk.current_ptr_;
            std::size_t free_size = chunk.free_bytes_;
            // align checks not just if it has enough space for alignment but also if there is
            // enough space to add the element inside
            void *maybe_aligned_ptr = std::align(alignment, size, maybe_enough, free_size);
            if(maybe_aligned_ptr != nullptr) {
                chunk.free_bytes_ = free_size - size;
                chunk.current_ptr_ = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(maybe_aligned_ptr)+size);
                chunk.num_allocations_+=1;
                return maybe_aligned_ptr;
            }

            return nullptr;
        }

        void deallocate(void *ptr){
            // check if it is top level chunk
            // if not mark that region as deallocated
            // no op for now
        }
        void release() {
            /// todo think about fragmentation
            ///
            for(auto &chunk: chunks_) {
                memory_pool_allocator_.deallocate(chunk.start_ptr_);
            }
        }

        ~ChunkAllocator() {
            release();
        }

    private:
        struct TopLevelPointer{
            uint64_t *ptr_;
            bool is_top_level_;
        };


        struct Chunk{
            explicit Chunk(void *start_ptr,
            void *current_ptr,
            std::size_t free_bytes,
            std::size_t num_allocations): start_ptr_(start_ptr), current_ptr_(current_ptr), free_bytes_(free_bytes), num_allocations_(num_allocations){}

            void *start_ptr_;
            void *current_ptr_;
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