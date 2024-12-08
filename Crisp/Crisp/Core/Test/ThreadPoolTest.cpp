#include <Crisp/Core/ThreadPool.hpp>

#include <gmock/gmock.h>

namespace crisp {
namespace {

TEST(ThreadPoolTest, Basic) {
    ThreadPool threadPool;

    for (uint32_t i = 0; i < 1000000; ++i) {
        threadPool.schedule([]() {});
    }
}
} // namespace
} // namespace crisp