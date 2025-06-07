//
// Created by antonio-filipovic on 4/26/25.
//

#include <memory>
#include <iostream>
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <vector>
#include <type_aware_allocator.hpp>
#include <malloc.h>
#include <unistd.h>

struct malloc_chunk {
    // size of previous chunk, only used if it is free
    std::size_t prev_size_;
    // size of current chunk, used to traverse free chunks
    std::size_t free_size_;

    malloc_chunk* fd_; // forward pointer
    malloc_chunk *bd_; // backward pointer
};

long sz = sysconf(_SC_PAGESIZE);

struct Widget{
    Widget(double double_val, int int_val): double_(double_val), int_(int_val) {}

    double double_;
    int int_;
    char array[1024*1024 *16]{}; // this default constructs each value, hence the reason for writing
};



struct WidgetSmall{
    char array[16]{};
};


void print_addresses(void *p, std::string_view text) {
    uint64_t fp_uint;
    // can I use reinterpret cast here? or it requires std::memcpy to avoid the issue
    // of incorrectly doing type punning
    std::memcpy(&fp_uint, &p, sizeof(p));



    std::cout <<  text << fp_uint << std::endl;
}

void print_difference_addresses(void *p1, void *p2, std::string_view text) {
    uint64_t p1_uint, p2_uint;
    // can I use reinterpret cast here? or it requires std::memcpy to avoid the issue
    // of incorrectly doing type punning
    std::memcpy(&p1_uint, &p1, sizeof(p1));
    std::memcpy(&p2_uint, &p2, sizeof(p2));


    std::cout <<  text << (p2_uint - p1_uint) << " B, " <<  (p2_uint - p1_uint) / 1024 << " kB" << std::endl;
}

void test_big_page_alloc() {
    //////
    ///
    ///
    auto *x = new WidgetSmall;
    auto *x2 = new WidgetSmall;
    uint64_t x_uint, x2_uint;
    std::memcpy(&x_uint, &x, sizeof(x));
    std::memcpy(&x2_uint, &x2, sizeof(x2));

    std::cout << sizeof(Widget) << ", alignment: " << alignof(Widget) << std::endl;

    std::cout << x_uint << std::endl;
    std::cout << x2_uint << std::endl;
    std::cout << x2_uint - x_uint << std::endl;

    std::cout << x << std::endl;
    std::cout << x2 << std::endl;

    delete x;
    delete x2;
    //int x_diff = x2 - x;

    //std::cout << x_diff << std::endl;

    void *p = malloc(sizeof(WidgetSmall)*2);
    auto *fp = new (p) WidgetSmall{} ;
    auto *fp2 = new (fp+1) WidgetSmall{};



    uint64_t fp_uint, fp2_uint;
    // can I use reinterpret cast here? or it requires std::memcpy to avoid the issue
    // of incorrectly doing type punning
    std::memcpy(&fp_uint, &fp, sizeof(fp));
    std::memcpy(&fp2_uint, &fp2, sizeof(fp2));



    std::cout << fp_uint << std::endl;
    std::cout << fp2_uint << std::endl;
    std::cout << fp2_uint - fp_uint << std::endl;


    for(char &i : fp->array) {
        i='a';
    }

    for(char &i : fp2->array) {
        i='b';
    }


    // TODO ASAN does not report out of bounds issue?
    // maybe because malloc grants much more memory -> TODO how much
    for(int i=0; i < 33; i++) {
        std::cout << fp->array[i] << std::endl;
    }
    fp->array[500] = 'b';
    //fp->array[1024*1024] = 'b'; // only fails here on ASAN
    std::cout << fp2->array[15] << fp->array[15] << std::endl;

    free(p);
}

void test_std_allocator() {
    // std::allocator is bad: https://www.youtube.com/watch?v=LIb3L4vKZ7U&ab_channel=CppCon
    // memory alignment is possible with the use of alignof as far as I understand
    // std::align is one more thing that helps with alignment: https://en.cppreference.com/w/cpp/memory/align


    std::allocator<Widget> widget_allocator;
    std::cout << "test" << std::endl;

    Widget *memory_start = widget_allocator.allocate(2);
    auto *widget_memory = memory_start;
    print_addresses(widget_memory, "first widget memory: ");
    widget_allocator.construct(widget_memory, 2.0, 1);

    //std::construct_at(&s[i], i + 42);

    std::cout << "double: " << widget_memory->double_ << ", int: "<< widget_memory->int_ << std::endl;

    for(size_t i=0; i<1024*16; i+=100) {
        widget_memory->array[i]='a';
    }
    // pointer arithmetic
    ++widget_memory;
    print_addresses(widget_memory, "next widget memory: ");
    print_difference_addresses(memory_start, widget_memory, "diff between widget memory addresses: ");
    //assert(new_address==widget_memory+sizeof(Widget));
    widget_allocator.construct(widget_memory, 2.0, 1);

    std::cout << "double: " << widget_memory->double_ << ", int: "<< widget_memory->int_ << widget_memory->array[100*20] << std::endl;

    widget_allocator.deallocate(memory_start, 2);
}


void test_malloc() {
    // how much memory does malloc allocate
    /// https://lemire.me/blog/2024/06/27/how-much-memory-does-a-call-to-malloc-allocates/
    ///
    // malloc always allocates on boundary of 16 which is std::max_align_t
    void *p = malloc( 1);
    auto size = malloc_usable_size(p);
    print_addresses(p, "address: ");
    std::cout << size << std::endl;


    void *p2 = malloc( 1);
    auto size2 = malloc_usable_size(p);
    print_addresses(p2, "address: ");
    std::cout << size2 << std::endl;
    //*static_cast<char*>(p) = 'a';

    // std::cin >> done;
    // std::cout << p << std::endl;
    // std::cout << "allocated memory done" << std::endl;

    free(p);
    free(p2);
}

struct Temporary {
    double d;
    float f;
    int i;
};


struct BigChar {
    //256MB
    std::array <char,256*1024*1024> array{};
};
static_assert(sizeof(BigChar)==256*1024*1024); // 256MB


struct SmallChar {
    std::array <char,1*4096-100> array{}; // 1kB
};


struct MiniStruct {
    double double_;
    float float_;
    int int_;
};
static_assert(alignof(MiniStruct)==8);
static_assert(sizeof(MiniStruct)==16);




void allocated_memory(){
    std::vector<BigChar> real_array;
    real_array.reserve(4);

    // this part only asks from virtual memory but it is not marked as used just yet
    std::vector<BigChar*> pointers;
    pointers.reserve(4096);


    // only here in each iteration we see rise in the RES memory
    for(size_t i{0}; i<4; i++) {
        real_array.emplace_back();
    }

    for(auto &array:real_array) {
        for(size_t i{0}; i<array.array.size(); i++) {
            array.array[i]='b';
        }
    }


}


void allocate_different_sized_objects_page_arena() {
    /**
     * This experiment shows what happens when you try to access misaligned memory
     * Accessing misaligned memory leads to undefined behavior.
     */

    // this is aligned on page boundary
    malloc_info(0, stdout);
    // when it is not page aligned in that case we use 2 pages same as in case we allocate page aligned one page
    void *malloced_memory = std::malloc(4096-1024);
    memset(malloced_memory, 0, 516/sizeof(int));

    std::size_t allocated_size = sysconf(_SC_PAGESIZE);
    void *aligned_memory = std::aligned_alloc(sysconf(_SC_PAGESIZE), allocated_size);
    //void *aligned_memory = std::malloc(sysconf(_SC_PAGESIZE));
    // 4104 which makes sense why we see 2 pages allocated or 8 kB used
    std::size_t size_test = malloc_usable_size(aligned_memory);
    malloc_info(0, stdout);


    //print_addresses(aligned_memory, "aligned memory: ");

    auto *temp_char = new(aligned_memory) SmallChar();
    memset(&temp_char->array, 0, temp_char->array.size()/sizeof(int));
    memset(aligned_memory, 0, allocated_size/sizeof(int));

    malloc_info(0, stdout);



    // malloc does not properly align memory on page boundary
    // void *malloced_memory = malloc( 4096);
    //
    // // TODO(afilipovic) check that this allocates many pages
    // for(size_t byte{0}; byte < 4096; byte+=1024) {
    //     reinterpret_cast<char*>(malloced_memory)[byte] = 'a';
    // }


    //print_addresses(malloced_memory, "regular memory: ");

    // 1. how to allocate now temporary objects in aligned memory
    // 2. what happens if we wrongly align object -> this is undefined behavior
    // 3. use ubsan to check for misaligned access

    // for(size_t byte_offset{0}; byte_offset < 1024*4096; byte_offset+=1024) {
    //     std::cout << pointers[byte_offset]->array[0] << std::endl;
    // }


    ///// This is part of the experiment to get wrongly aligned memory
    auto new_aligned_memory = static_cast<void*>( static_cast<char*>(aligned_memory) + sizeof(SmallChar));

    print_addresses(new_aligned_memory, "new memory after allocation of temp char: ");
    // add part to print memory block like j turner did in video

    std::size_t alignment = alignof(SmallChar);

    std::size_t size_left = allocated_size - sizeof(SmallChar);
    std::size_t correct_size = sizeof(MiniStruct);
    std::cout << "size: " << correct_size << ", alignment: " << alignment <<  std::endl;

    /// commenting out this line makes memory non aligned and UBSAN throws memory alignment error
    std::align(alignof(MiniStruct), correct_size, new_aligned_memory,size_left);
    print_addresses(new_aligned_memory, "correctly aligned memory: ");

    auto *mini_struct = new(new_aligned_memory) MiniStruct{0, 0, 0};
    std::cout << mini_struct->double_ << std::endl;

    mini_struct->~MiniStruct();

    free(aligned_memory);
    free(malloced_memory);
}



void test_vector_allocator() {

    std::vector<int, SAllocator<int>> my_vector;
    my_vector.reserve(1);
    my_vector.emplace_back(1);
    my_vector.emplace_back(2);


}


int main(){


    //test_std_allocator();

    //test_big_page_alloc();


    //test_vector_allocator();

    //test_malloc();

    allocate_different_sized_objects_page_arena();
    return 0;
}

