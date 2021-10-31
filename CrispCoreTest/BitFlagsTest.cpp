#include "gtest/gtest.h"

#include <CrispCore/BitFlags.hpp>

namespace
{
    enum class MyOptions
    {
        Red = 0x01,
        Green = 0x02,
        Blue = 0x04,

        Yellow = Red | Green,
        Cyan = Green | Blue,
        Magenta = Red | Blue
    };

    DECLARE_BITFLAG(MyOptions);
}

TEST(BitFlagsTest, Comparisons)
{
    MyOptionsFlags flags;

    EXPECT_TRUE(flags == MyOptionsFlags());

    flags |= MyOptions::Red;

    EXPECT_TRUE(flags == MyOptionsFlags(MyOptions::Red));
    EXPECT_TRUE(flags == MyOptions::Red);

    flags |= MyOptions::Red;
    EXPECT_EQ(flags, MyOptions::Red);

    EXPECT_NE(flags, MyOptions::Green);

    flags |= MyOptions::Green;
    EXPECT_EQ(flags, MyOptions::Red | MyOptions::Green);
}

TEST(BitFlagsTest, BitwiseOperations)
{
    MyOptionsFlags flags;

    EXPECT_TRUE(flags == MyOptionsFlags());

    flags |= MyOptions::Red;

    EXPECT_TRUE(flags == MyOptionsFlags(MyOptions::Red));
    EXPECT_TRUE(flags == MyOptions::Red);

    flags |= MyOptions::Red;
    EXPECT_EQ(flags, MyOptions::Red);

    EXPECT_NE(flags, MyOptions::Green);

    flags |= MyOptions::Green;
    EXPECT_EQ(flags, MyOptions::Red | MyOptions::Green);

    flags.disable(MyOptions::Red);
    EXPECT_EQ(flags, MyOptions::Green);
    EXPECT_TRUE(flags & MyOptions::Green);
    EXPECT_FALSE(flags & MyOptions::Red);

    flags |= MyOptions::Yellow;
    EXPECT_FALSE(flags & MyOptions::Blue);
}
