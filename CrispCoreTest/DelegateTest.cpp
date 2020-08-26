#include "gtest/gtest.h"

#include "../CrispCore/CrispCore/Delegate.hpp"

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
}

TEST(DelegateTest, Basic)
{
    DelegateTester tester;

    Delegate<void, int> del = Delegate<void, int>::fromMemberFunction<&DelegateTester::onEventTriggered>(&tester);
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

    Delegate<void, int> del = Delegate<void, int>::fromMemberFunction<&DelegateTester::onEventTriggered>(&tester);
    del(10);

    EXPECT_TRUE(del.isFromObject(&tester));
}

TEST(DelegateTest, Comparison)
{
    DelegateTester tester;
    DelegateTester tester2;

    Delegate<void, int> del = Delegate<void, int>::fromMemberFunction<&DelegateTester::onEventTriggered>(&tester);
    Delegate<void, int> del2 = Delegate<void, int>::fromMemberFunction<&DelegateTester::onEventTriggered>(&tester2);
    del(10);

    EXPECT_TRUE(del != del2);
    EXPECT_TRUE(del == del);
    EXPECT_TRUE(del2 == del2);
}