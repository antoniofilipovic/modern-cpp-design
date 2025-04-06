cmake_minimum_required(VERSION 3.24)

project(
        ModernCppDesign
        VERSION 0.1
        LANGUAGES CXX)

## do i need to set TARGET property or global one?
set_property(GLOBAL PROPERTY CXX_STANDARD 20)

## includes directory which you need
add_subdirectory(src)

