#include <cassert>

#include "afmalloc.hpp"
#include <string>
#include <iostream>
#include <memory_resource>

struct noisy_allocator : std::pmr::memory_resource {

  explicit noisy_allocator() : af_malloc(false, 0) {}

  void* do_allocate(std::size_t bytes, std::size_t alignment) override
  {
    std::cout << "+ Allocating " << bytes << " bytes @ ";
    assert(alignment == 1);
    void* p = af_malloc.malloc(bytes);
    std::cout << p << '\n';
    return p;
  }

  void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
  {
    af_malloc.free(p);
  }

  [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
  {
    return std::pmr::new_delete_resource()->is_equal(other);
  }

  AFMalloc af_malloc;
};

int main(){
  AFMalloc af_malloc{false, 0};
  void *ptr = af_malloc.malloc(10);
  char *char_ptr = reinterpret_cast<char*>(ptr);
  char_ptr[0] = 'a';

  af_malloc.free(ptr);
  return 0;
}