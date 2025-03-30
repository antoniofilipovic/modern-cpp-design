add_library(smart_pointer smart_pointer.cpp)

# PUBLIC is when you want to use your own header libraries
# INTERFACE is for some obscure case, not getting into the details (header only lib includes header?)
target_include_directories(smart_pointer PUBLIC ../../include/modern_cpp_design/smart_pointer)