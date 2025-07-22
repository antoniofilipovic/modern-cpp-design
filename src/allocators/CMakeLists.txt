add_library(allocators STATIC)

target_sources(allocators
        PUBLIC
        ../../include/modern_cpp_design/allocators/
        PRIVATE
        page_size_allocator.cpp
        memory_pool_allocator.cpp
)
target_include_directories(allocators PUBLIC ../../include/modern_cpp_design/allocators)


add_library(afmalloc STATIC)

target_sources(afmalloc
        PUBLIC
        ../../include/afmalloc/
        PRIVATE
        afmalloc.cpp
)
target_include_directories(afmalloc PUBLIC ../../include/afmalloc)
