
#include <Crisp/Core/SlotMap.hpp>

#include <gmock/gmock.h>

#include <numeric>

namespace crisp::test
{
namespace
{
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(SlotMapTest, Basic)
{
    SlotMap<std::string> slotMap;

    const auto k1 = slotMap.insert("red");
    EXPECT_THAT(k1.id, 0);
    EXPECT_THAT(k1.generation, 0);

    EXPECT_THAT(slotMap, SizeIs(1));
    EXPECT_EQ(slotMap[k1], "red");

    EXPECT_TRUE(slotMap.erase(k1));

    const auto k2 = slotMap.insert("red");
    EXPECT_THAT(k2.id, 0);
    EXPECT_THAT(k2.generation, 1);
}

TEST(SlotMapTest, Push)
{
    SlotMap<std::string> slotMap;

    std::vector<decltype(slotMap)::HandleType> handles;

    for (uint32_t i = 0; i < 10; ++i)
    {
        handles.push_back(slotMap.insert(std::to_string(i)));
    }
    EXPECT_THAT(slotMap, SizeIs(10));

    for (const auto& h : handles)
    {
        slotMap.erase(h);
    }
    EXPECT_THAT(slotMap, IsEmpty());
}

TEST(SlotMapTest, HandleType)
{
    Handle handle;
    handle.id = 10;
    handle.generation = 0;
    static_assert(sizeof(handle) == sizeof(uint32_t));
}

} // namespace
} // namespace crisp::test