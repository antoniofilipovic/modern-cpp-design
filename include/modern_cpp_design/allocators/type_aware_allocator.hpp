

#pragma once
#include <cstdlib>

template <typename T>
class SAllocator{
  public:
    typedef T value_type;
    SAllocator() = default;

    template<class U>
    constexpr explicit SAllocator(const SAllocator <U>&) noexcept {}


    void report_allocations(void *p, std::size_t n) {
      static std::size_t alignment = alignof(std::max_align_t);

      uint64_t fp_uint;
      // can I use reinterpret cast here? or it requires std::memcpy to avoid the issue
      // of incorrectly doing type punning
      std::memcpy(&fp_uint, &p, sizeof(p));


      std::cout << "allocated on address: " << fp_uint << ", " << n << " B" << ", " << n/1024 << " kB." << " Address divisible with  " << alignment << " :" << std::boolalpha << ((fp_uint % alignment) == 0)  << std::endl;

    }

    void report_deallocations(void *p, std::size_t n) {
      uint64_t fp_uint;
      // can I use reinterpret cast here? or it requires std::memcpy to avoid the issue
      // of incorrectly doing type punning
      std::memcpy(&fp_uint, &p, sizeof(p));

      std::cout << "deallocated on address: " << fp_uint << ", bytes: " << n << std::endl;
    }

    [[nodiscard]]  T *allocate(std::size_t n){
      // why not use aligned alloc
      // this may not be aligned on T boundery and we have an issue
      // todo check how we can check if memory is not properly aligned?
      // check if we can get signal from the system on not aligned access

      // todo check if we can allocate or throw out of bounds

      if(auto *p = std::malloc(n * sizeof(T)); p != nullptr) {
        report_allocations(p, n * sizeof(T));
        return static_cast<T*>(p);
      }
      throw std::bad_alloc{};
    }


    void deallocate(void *p, std::size_t n){
      report_deallocations(p,n * sizeof(T));
      std::free(p);
    }


};