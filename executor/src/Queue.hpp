#pragma once

#include <array>
#include <stddef>
#include <mutex>

namespace concurrency {
inline namespace v1 {
namespace detail {
template <class Task>
class BoundedQueue {
 public:
  BoundedQueue(std::size_t maxSize = 0x100) : queue(maxSize), max(maxSize) {
  }
  bool push(Task&& t) noexcept {
    auto guard = std::unique_lock<std::mutex>(mute, std::try_lock); 
    if (!guard.owns_lock()) return false;
    if (hasSpace()) return false;
    queue[++write % N] = std::move(t);
    cv.notify_one();
    return true;
  }
  bool tryPop(Task& t) noexcept {
    auto guard = std::unique_lock<std::mutex>(mute, std::try_lock); 
    if (!guard.owns_lock() || !hasData()) return false;
    t = std::move(queue[++read % N]);
    return true;
  }
  bool pop(Task& t) noexcept {
    auto guard = std::unique_lock<std::mutex>(mute, std::try_lock); 
    if (!guard.owns_lock()) return false;
    if (!hasData()) {
      cv.wait(guard, [this] { hasData(); });
    }
    t = std::move(queue[++read % N]);
    return true;
  }
 private:
  // |||r******w|||||
  // ******w|||||r***
  // ***********wr***
  // |||r***********w
  // r**************w
  // ||||||||||||rw||
  bool hasSpace() const noexcept {
    next = (write + 1) % N;
    if (next == read) return false;
    return true;
  }
  bool hasData() const noexcept {
    next = (read + 1) % N;
    if (next == write || write < 0) return false;
    return true;
  }
  std::mutex mute;
  std::condition_variable cv;
  std::vector<Task> queue;
  std::int32_t read = -1;
  std::int32_t write = -1;
  std::uint32_t max;
};
}

template <class Task, std::size_t N>
class BoundedQueue {
 public:
  Queue() {}
  bool push(Task&& t, unsigned tid) noexcept {
    auto firstQueue = tid % N;
    auto lastQueue = (firstQueue - 1) % N;
    for (unsigned i = firstQueue; i != lastQueue; i = (i + 1) % N) {
      if (queues[tid % N].push(std::forward<Task>(t))) return true;
    }
    return false;
  }
  bool tryPop(Task& t, unsigned tid) noexcept {
    auto firstQueue = tid % N;
    auto lastQueue = (firstQueue - 1) % N;
    for (unsigned i = firstQueue; i != lastQueue; i = (i + 1) % N) {
      if (queues[tid % N].tryPop(t)) return true;
    }
    return false;
  }
  bool pop(Task& t, unsigned tid) noexcept {
    if (tryPop(t, tid)) return true;
    queues[tid % N].pop(t);
    return true;
  }
 private:
  std::array<detail::Queue, N> queues;
};
}
}
