#pragma once

#include <cstddef>
#include <thread>
#include <vector>
#include <atomic>
#include <Queue.hpp>

namespace concurrency {
inline namespace v1 {
template <class Task, std::size_t N>
class Executor {
 public:
  Executor() {
    for (auto& thr : threads) {
      thr = std::thread([this] {
        auto tid = std::hash<std::thread::id>()(std::this_thread::get_id());
        while (running) {
          try {
            Task task;
            while (queue.tryPop(task, tid)) {
              task();
            }
          } catch (std::exception& e) {
          }
        }
      });
    }
  }
  ~Executor() {
    running = false;
    for (auto& thr : threads)
      if (thr.joinable()) thr.join();
    assert(queue.size() == 0);
  }
  template <class Callable>
  bool submit(Callable&& t, std::size_t tid) noexcept {
    return queue.push(std::forward<Callable>(t), tid);
  }

  std::size_t queue_size() const {
    return queue.size();
  }
 private:
  std::array<std::thread, N> threads;
  std::atomic<bool> running = {true};
  BoundedQueue<Task, N> queue;
};
template <class Task, std::size_t N>
bool submit(Task&& t) {
  static Executor<Task, N> executor;
  return executor.submit(std::forward<Task>(t));
}
}
}
