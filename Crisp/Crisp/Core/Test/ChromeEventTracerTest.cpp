#include <Crisp/Core/ChromeEventTracer.hpp>

#include <gmock/gmock.h>

namespace crisp::test {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;

auto ScopeEventIs(const std::string_view name, ScopeEventType type) {
    return AllOf(Field(&ScopeEvent::name, name), Field(&ScopeEvent::type, type));
}

TEST(ChromeEventTracerTest, Basic) {
    {
        CRISP_TRACE_SCOPE("Test 1");
        std::this_thread::sleep_for(std::chrono::duration<double>(1.0));
    }

    auto& events = detail::getCpuContext().getTracedEvents();

    EXPECT_THAT(
        events, ElementsAre(ScopeEventIs("Test 1", ScopeEventType::Begin), ScopeEventIs("Test 1", ScopeEventType::End)));
    EXPECT_GE(events[1].timestamp - events[0].timestamp, 1e9);
    EXPECT_LT(events[1].timestamp - events[0].timestamp, 2e9);
}

TEST(ChromeEventTracerTest, WritesFiles) {
    {
        CRISP_TRACE_SCOPE("Test 1");
        std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
        {
            CRISP_TRACE_SCOPE("Test 2");
            std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
        }
    }

    serializeTracedEvents("D:/Projects/Crisp/Output/test.trace");
}

} // namespace
} // namespace crisp::test