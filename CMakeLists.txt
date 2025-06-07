cmake_minimum_required(VERSION 3.24)

project(
        ModernCppDesign
        VERSION 0.1
        LANGUAGES CXX)

## do i need to set TARGET property or global one?
find_program(CLANG_FOUND clang)
find_program(CLANGXX_FOUND clang++)
if (CLANG_FOUND AND CLANGXX_FOUND)
    set(CMAKE_C_COMPILER ${CLANG_FOUND})
    set(CMAKE_CXX_COMPILER ${CLANGXX_FOUND})
else()
    message(FATAL_ERROR "Couldn't find clang and/or clang++!")
endif()

set_property(GLOBAL PROPERTY CXX_STANDARD 20)
option(UBSAN "Build with Undefined Behaviour Sanitizer" OFF)


# is this correct
# does this exist? -fsanitize=bounds-strict -fsanitize=bounds
if (UBSAN)
    #    set(
    #            CMAKE_CXX_FLAGS
    #            "${CMAKE_CXX_FLAGS} -Werror -fsanitize=undefined -fsanitize=address -fsanitize=bounds"
    #    )
    #    target_link_options(allocators
    #            BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address  PUBLIC -fsanitize=bounds
    #    )

    # Add this behind a flag
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize=vptr)
    add_link_options(-fsanitize=undefined -fno-sanitize=vptr)
endif()

## includes directory which you need
add_subdirectory(src)



