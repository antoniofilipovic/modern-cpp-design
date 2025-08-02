include(GoogleTest)

add_executable(test_page_allocator test_page_allocator.cpp)
target_include_directories(test_page_allocator PUBLIC ../include/modern_cpp_design/allocators)
target_link_libraries(test_page_allocator GTest::gtest_main allocators)



add_executable(test_memory_pool_allocator test_memory_pool_allocator.cpp)
target_include_directories(test_memory_pool_allocator PUBLIC ../include/modern_cpp_design/allocators)
target_link_libraries(test_memory_pool_allocator GTest::gtest_main allocators)


#gtest_discover_tests(page_allocator_test)


add_executable(test_afmalloc test_afmalloc.cpp)
target_include_directories(test_afmalloc PUBLIC ../include/afmalloc)
target_link_libraries(test_afmalloc GTest::gtest_main afmalloc)