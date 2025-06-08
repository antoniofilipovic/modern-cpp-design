
add_executable(custom_allocator custom_allocator.cpp ../../include/modern_cpp_design/allocators/chunk_allocator.hpp ../../include/modern_cpp_design/allocators/memory_pool_allocator.hpp)
target_include_directories(custom_allocator PUBLIC ../../include/modern_cpp_design/allocators)

