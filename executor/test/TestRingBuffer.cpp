#include <gtest/gtest.h>
#include <RingBuffer.hpp>

using namespace concurrency;

class RingBufferTests : public testing::Test {
 public:
  RingBufferTests() : rb(size) {}
  std::size_t size = 0x100;
  RingBuffer<int> rb;
};

TEST_F(RingBufferTests, create) {
  ASSERT_EQ(rb.size(), size);
}

TEST_F(RingBufferTests, push) {
  ASSERT_TRUE(rb.push(1));
  ASSERT_EQ(rb.size(), size - 1);
}

TEST_F(RingBufferTests, pushAndPop) {
  int arg = 0xbeef;
  ASSERT_TRUE(rb.push(arg));
  ASSERT_EQ(rb.size(), size - 1);
  int res = 0;
  ASSERT_TRUE(rb.pop(res));
  ASSERT_EQ(arg, res);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
