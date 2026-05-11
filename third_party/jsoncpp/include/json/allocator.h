/// JsonCpp allocator
#ifndef JSON_ALLOCATOR_H_INCLUDED
#define JSON_ALLOCATOR_H_INCLUDED

#include <cstddef>
#include <cstdlib>
#include <new>

namespace Json {

template <typename T>
class SecureAllocator {
public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  SecureAllocator() noexcept = default;
  ~SecureAllocator() noexcept = default;

  template <typename U>
  SecureAllocator(const SecureAllocator<U>&) noexcept {}

  pointer allocate(size_type n) {
    pointer p = static_cast<pointer>(std::malloc(n * sizeof(T)));
    if (!p) throw std::bad_alloc();
    return p;
  }

  void deallocate(pointer p, size_type) noexcept { std::free(p); }

  template <typename U>
  struct rebind {
    using other = SecureAllocator<U>;
  };
};

template <typename T, typename U>
bool operator==(const SecureAllocator<T>&, const SecureAllocator<U>&) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const SecureAllocator<T>&, const SecureAllocator<U>&) noexcept {
  return false;
}

} // namespace Json

#endif // JSON_ALLOCATOR_H_INCLUDED
