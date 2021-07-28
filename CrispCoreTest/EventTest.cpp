#include "gtest/gtest.h"

#include "CrispCore/Event.hpp"
#include "CrispCore/Delegate.hpp"

using namespace crisp;

namespace
{
    static int copies = 0;
    class EventData
    {
    public:
        EventData(std::string msg, double x, double y) : message(msg), x(x), y(y) {}

        EventData(const EventData& ev)
            : message(ev.message)
            , x(ev.x)
            , y(ev.y)
        {
            copies++;
            std::cout << "Copy ctor\n";
        }

        std::string message;
        double x;
        double y;
    };

    struct DelegateTester
    {
        std::string message;
        int state = 0;

        int triggerCounter = 0;

        void onEventTriggered(int newStateValue, std::string msg)
        {
            message = msg;
            state = newStateValue;
            ++triggerCounter;
        }

        void onEventData(const EventData& evData)
        {
            message = evData.message;
            state = evData.x * evData.y;
        }
    };

    void printStuff(const std::string& message)
    {
    }
}

TEST(EventTest, Subscriptions)
{
    Event<int, std::string> customEvent;
    DelegateTester tester;

    const auto del = createDelegate<&DelegateTester::onEventTriggered>(&tester);

    customEvent += del;

    EXPECT_EQ(customEvent.getSubscriberCount(), 1);

    customEvent += del;
    customEvent += del;
    customEvent += del;
    EXPECT_EQ(customEvent.getSubscriberCount(), 1);

    customEvent(10, "Aloha");

    EXPECT_EQ(tester.state, 10);

    customEvent -= del;
    EXPECT_EQ(customEvent.getSubscriberCount(), 0);
}

TEST(EventTest, Disconnects)
{
    Event<const EventData&> event;
    DelegateTester tester;

    const auto del = createDelegate<&DelegateTester::onEventData>(&tester);

    event += del;
    event += {&tester, [](const EventData& data) {}};
    EXPECT_EQ(event.getSubscriberCount(), 2);
    EXPECT_EQ(event.getDelegateCount(), 1);
    EXPECT_EQ(event.getFunctorCount(), 1);

    EventData data("Hello", 10, 20);

    event(data);

    EXPECT_EQ(tester.state, 200);

    event.unsubscribe(&tester);
    EXPECT_EQ(event.getSubscriberCount(), 0);
}

TEST(EventTest, StaticFunctions)
{
    Event<const std::string&> event;

    const auto freeDelegate = createDelegate<printStuff>();
    freeDelegate("Hello World!");

    event.subscribeStatic<&printStuff>();
    EXPECT_EQ(event.getSubscriberCount(), 1);
}

TEST(EventTest, Autodisconnect)
{
    Event<const std::string&> event;
    event.subscribeStatic<&printStuff>();

    {
        const auto handler = event.subscribe([](const std::string& a) { std::cout << a.size() << std::endl; });
        EXPECT_EQ(event.getSubscriberCount(), 2);
    }

    EXPECT_EQ(event.getSubscriberCount(), 1);
}
