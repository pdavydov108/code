#pragma once

#include <cstddef>
#include <thread>
#include <vector>
#include <Queue.hpp>

namespace concurrency {
inline namespace v1 {
template <class Task, std::size_t N>
class Executor {
 public:
  Executor() {
    for (auto& thr : threads) {
      thr = std::move([this] {
        try {
          Task task;
          queue.pop(task);
          task();
        } catch (std::exception& e) { }
      });
     }
   }
   bool submit(Task&& t) noexcept {
     return queue.push(std::forward<Task>(t));
   }
 private:
   std::array<std::thread, N> threads;
   BoundedQueue<Task, N> queue;
};
template <class Task, std::size_t N>
bool submit(Task&& t) {
  static Executor<Task, N> executor;
  return executor.submit(std::forward<Task>(t));
}
}
}
