#pragma once

#include <cstdint>
#include <memory>

namespace concurrency {
inline namespace v1 {
// |||r******w|||||
// ******w|||||r***
// ***********wr***
// |||r***********w
// r**************w
// ||||||||||||rw||
template <class T>
class RingBuffer {
 public:
  RingBuffer(std::size_t _size) : max(_size), memory(new T[max]) {}
  std::size_t pushArray(T* ptr, std::size_t count) noexcept {
    assert(ptr && count > 0 && count <= max);
    if (write > read) {
      auto space = (max - write);
      std::move(ptr, ptr + space > count ? count : space, memory.get() + write);
      write += max - write;
      count -= max - write;
    }
  }
  template <class ConvertibleT>
  bool push(ConvertibleT&& t) noexcept {
    memory[++write % max] = std::move(t);
    return true;
  }
  bool tryPop(T& t) noexcept {
    t = std::move(memory[++read % max]);
    return true;
  }
  bool pop(T& t) noexcept {
    t = std::move(memory[++read % max]);
    return true;
  }
  std::size_t size() const noexcept {
    auto diff = write - read;
    return diff > 0 ? diff : (max - write) + read;
  }

 private:
  std::int64_t read = 0;
  std::int64_t write = 1;
  const std::uint64_t max;
  std::unique_ptr<T[]> memory;
};
}
}
