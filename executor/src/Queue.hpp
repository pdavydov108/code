#pragma once

#include <array>
#include <cstddef>
#include <mutex>
#include <condition_variable>
#include <numeric>
#include <RingBuffer.hpp>

namespace concurrency {
inline namespace v1 {
namespace detail {
template <class Task>
class BoundedQueue {
 public:
  BoundedQueue(std::size_t maxSize = 0x100) : queue(maxSize), max(maxSize) {
  }
  template <class Callable>
  bool push(Callable&& t) noexcept {
    auto guard = std::unique_lock<std::mutex>(mute, std::try_to_lock); 
    if (!guard.owns_lock()) return false;
    if (hasSpace()) return false;
    queue[++write % max] = std::move(t);
    cv.notify_one();
    return true;
  }
  bool tryPop(Task& t) noexcept {
    auto guard = std::unique_lock<std::mutex>(mute, std::try_to_lock); 
    if (!guard.owns_lock() || !hasData()) return false;
    t = std::move(queue[++read % max]);
    return true;
  }
  bool pop(Task& t) noexcept {
    auto guard = std::unique_lock<std::mutex>(mute, std::try_to_lock); 
    if (!guard.owns_lock()) return false;
    if (!hasData()) {
      cv.wait(guard, [this] { hasData(); });
    }
    t = std::move(queue[++read % max]);
    return true;
  }
  std::size_t size() const {
    return read < write ? write - read - 1 : queue.size() - read + write;
  }
 private:
  bool hasSpace() const noexcept {
    auto next = (write + 1) % max;
    if (next == read) return false;
    return true;
  }
  bool hasData() const noexcept {
    auto next = (read + 1) % max;
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
  BoundedQueue() {}
  template <class Callable>
  bool push(Callable&& t, unsigned tid) noexcept {
    auto firstQueue = tid % N;
    auto lastQueue = (firstQueue - 1) % N;
    for (unsigned i = firstQueue; i != lastQueue; i = (i + 1) % N) {
      if (queues[tid % N].push(std::forward<Callable>(t))) return true;
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
  std::size_t size() const {
    return std::accumulate(queues.begin(), queues.end(), 0ul, [] (auto&& l, auto&& r) { return l + r.size(); });
  }
 private:
  std::array<detail::BoundedQueue<Task>, N> queues;
};
}
}
