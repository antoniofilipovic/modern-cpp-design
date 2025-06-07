# this is the main part
# here we prepare running executable (target)
add_executable(pointer smart_pointer.cpp)
# this does the linking of the targets (executable and library)
# PUBLIC is when you want to use your own header libraries
# INTERFACE is for some obscure case, not getting into the details (header only lib includes header?)
target_include_directories(pointer PUBLIC ../include/modern_cpp_design/smart_pointer)


add_executable(typelists typelists.cpp)
target_include_directories(typelists PUBLIC ../include/modern_cpp_design/typelists)


add_executable(allocators malloc_playground.cpp)
target_include_directories(allocators PUBLIC ../include/modern_cpp_design/allocators)


add_subdirectory(allocation)