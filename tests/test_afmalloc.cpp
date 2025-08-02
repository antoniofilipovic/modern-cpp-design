#include <gtest/gtest.h>
#include <string>

#include "afmalloc.hpp"




TEST(AfMallocTest, TestAfMallocGet3Chunks){
    AFMalloc af_malloc{false, 0};

    void *ptr = af_malloc.malloc(10);
    char *first_str = reinterpret_cast<char *>(ptr);
    strcpy(first_str, "baba");

    void *second_ptr = af_malloc.malloc(10);
    char *second_str = reinterpret_cast<char *>(second_ptr);
    strcpy(second_str, "deda");


    void *third_ptr = af_malloc.malloc(10);
    char *third_str = reinterpret_cast<char *>(third_ptr);
    strcpy(third_str, "deda");

    af_malloc.free(ptr);
    af_malloc.free(second_ptr);
    af_malloc.free(third_ptr);


}