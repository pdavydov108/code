#include <gtest/gtest.h>
#include <Executor.hpp>

using namespace concurrency;

TEST(Test, Executor) {
  Executor<std::function<void()>, 1> executor; 
  ASSERT_EQ(executor.queue_size(), 0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
