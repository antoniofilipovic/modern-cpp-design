cmake_minimum_required(VERSION 3.24)

project(
        ModernCppDesign
        VERSION 0.1
        LANGUAGES CXX)

## do i need to set TARGET property or global one?
set_property(GLOBAL PROPERTY CXX_STANDARD 20)

## includes directory which you need
add_subdirectory(src)

# how to link src with include
## according to the docs I can link
#target_include_directories(shared_ptr include)

# this is the main part
# here we prepare running executable (target)
add_executable(main src/main.cpp)
# this does the linking of the targets (executable and library)
target_link_libraries(main smart_pointer)