#include <concepts>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include <Crisp/Utils/BitFlags.hpp>

namespace crisp {
enum class MyOptions : uint8_t {
    Red = 0x01,
    Green = 0x02,
    Blue = 0x04,

    Yellow = Red | Green,
    Cyan = Green | Blue,
    Magenta = Red | Blue,
};

DECLARE_BITFLAG(MyOptions);

static_assert(std::same_as<decltype(MyOptions::Red | MyOptions::Green), MyOptionsFlags>);
static_assert(!std::is_convertible_v<MyOptionsFlags, bool>);
static_assert(std::is_trivially_copyable_v<MyOptionsFlags>);
static_assert(sizeof(MyOptionsFlags) == sizeof(uint8_t));

constexpr auto kPrimaryOptions = MyOptions::Red | MyOptions::Green | MyOptions::Blue;
static_assert(kPrimaryOptions.containsAll(MyOptions::Yellow));
static_assert(kPrimaryOptions.getMask() == 0x07);

namespace test {

TEST(BitFlagsTest, ConstructsAndComparesTypedFlags) {
    MyOptionsFlags flags;
    EXPECT_TRUE(flags.empty());
    EXPECT_FALSE(static_cast<bool>(flags));

    flags |= MyOptions::Red;
    EXPECT_EQ(flags, MyOptionsFlags{MyOptions::Red});
    EXPECT_NE(flags, MyOptionsFlags{MyOptions::Green});
    EXPECT_TRUE(static_cast<bool>(flags));

    const auto yellow = MyOptions::Red | MyOptions::Green;
    EXPECT_EQ(flags | MyOptions::Green, yellow);
    EXPECT_EQ(MyOptions::Green | flags, yellow);
}

TEST(BitFlagsTest, DistinguishesAnyAndAllQueries) {
    const MyOptionsFlags flags = MyOptions::Yellow;

    EXPECT_TRUE(flags.contains(MyOptions::Red));
    EXPECT_TRUE(flags.containsAll(MyOptions::Yellow));
    EXPECT_TRUE(flags.containsAny(MyOptions::Magenta));
    EXPECT_FALSE(flags.containsAll(MyOptions::Magenta));
    EXPECT_FALSE(flags.containsAny(MyOptions::Blue));
}

TEST(BitFlagsTest, MutatesFlagsExplicitly) {
    MyOptionsFlags flags;

    flags.set(MyOptions::Red).set(MyOptions::Green);
    EXPECT_EQ(flags, MyOptionsFlags{MyOptions::Yellow});

    flags.reset(MyOptions::Red);
    EXPECT_EQ(flags, MyOptionsFlags{MyOptions::Green});

    flags.toggle(MyOptions::Green).toggle(MyOptions::Blue);
    EXPECT_EQ(flags, MyOptionsFlags{MyOptions::Blue});

    flags.set(MyOptions::Red, false);
    EXPECT_EQ(flags, MyOptionsFlags{MyOptions::Blue});

    flags.clear();
    EXPECT_TRUE(flags.empty());
}

TEST(BitFlagsTest, SupportsBitwiseOperationsWithoutLosingType) {
    const MyOptionsFlags yellow = MyOptions::Yellow;
    const MyOptionsFlags magenta = MyOptions::Magenta;

    EXPECT_EQ(yellow & magenta, MyOptionsFlags{MyOptions::Red});
    EXPECT_EQ(yellow ^ magenta, MyOptionsFlags{MyOptions::Cyan});

    auto flags = yellow;
    flags &= MyOptions::Magenta;
    EXPECT_EQ(flags, MyOptionsFlags{MyOptions::Red});
    flags ^= MyOptions::Blue;
    EXPECT_EQ(flags, MyOptionsFlags{MyOptions::Magenta});
}

TEST(BitFlagsTest, MakesRawMaskConversionExplicit) {
    constexpr auto flags = MyOptionsFlags::fromMask(0x05);
    static_assert(flags.containsAll(MyOptions::Magenta));

    EXPECT_EQ(flags.getMask(), 0x05);
}

} // namespace test
} // namespace crisp
