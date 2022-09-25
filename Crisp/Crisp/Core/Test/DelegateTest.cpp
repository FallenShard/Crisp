#include "gtest/gtest.h"

#include "../Crisp/Crisp/Core/Delegate.hpp"

using namespace crisp;

namespace
{
struct DelegateTester
{
    int state = 0;
    int triggerCounter = 0;

    void onEventTriggered(int newStateValue)
    {
        state = newStateValue;
        ++triggerCounter;
    }
};
} // namespace

TEST(DelegateTest, Basic)
{
    DelegateTester tester;

    const auto del = createDelegate<&DelegateTester::onEventTriggered>(&tester);
    del(10);
    EXPECT_EQ(tester.state, 10);

    del(10);
    del(10);
    del(10);
    EXPECT_EQ(tester.state, 10);
    EXPECT_EQ(tester.triggerCounter, 4);
}

TEST(DelegateTest, IsFromObject)
{
    DelegateTester tester;

    const auto del = createDelegate<&DelegateTester::onEventTriggered>(&tester);
    del(10);
    EXPECT_TRUE(del.isFromObject(&tester));

    DelegateTester tester2;
    EXPECT_FALSE(del.isFromObject(&tester2));
}

TEST(DelegateTest, Comparison)
{
    DelegateTester tester;
    DelegateTester tester2;

    const auto del = createDelegate<&DelegateTester::onEventTriggered>(&tester);
    const auto del2 = createDelegate<&DelegateTester::onEventTriggered>(&tester2);
    del(10);
    EXPECT_TRUE(del != del2);
    EXPECT_TRUE(del == del);
    EXPECT_TRUE(del2 == del2);
}