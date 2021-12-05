#include "gtest/gtest.h"

//#define __TBB_NO_IMPLICIT_LINKAGE 1
#include "tbb/concurrent_queue.h"

TEST(TbbTest, ConcurrentQueue)
{
    tbb::concurrent_queue<int> queue;
    queue.push(15);
    queue.push(30);
    EXPECT_EQ(queue.unsafe_size(), 2);
}