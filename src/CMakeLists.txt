add_subdirectory(allocators)

# this is the main part
# here we prepare running executable (target)
add_executable(pointer smart_pointer.cpp)
# this does the linking of the targets (executable and library)
# PUBLIC is when you want to use your own header libraries
# INTERFACE is for some obscure case, not getting into the details (header only lib includes header?)
target_include_directories(pointer PUBLIC ../include/modern_cpp_design/smart_pointer)


add_executable(typelists typelists.cpp)
target_include_directories(typelists PUBLIC ../include/modern_cpp_design/typelists)


add_executable(malloc_playground malloc_playground.cpp)
target_include_directories(malloc_playground PUBLIC ../include/modern_cpp_design/allocators)



add_executable(custom_allocator custom_allocator.cpp
        custom_allocator.cpp)
target_include_directories(custom_allocator PUBLIC ../include/modern_cpp_design/allocators)
target_link_libraries(custom_allocator allocators)




add_executable(afmalloc_playground afmalloc_playground.cpp)
target_include_directories(afmalloc_playground PUBLIC ../include/afmalloc)
target_link_libraries(afmalloc_playground afmalloc)
