#include <page_size_allocator.hpp>

int main(){
    memory::PageSizeAllocator page_size_allocator{};
    void *p = page_size_allocator.allocate(1,1);
    page_size_allocator.deallocate(p);
    return 0;
}