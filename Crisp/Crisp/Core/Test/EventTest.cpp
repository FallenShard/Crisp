

#include <Crisp/Core/Delegate.hpp>
#include <Crisp/Core/Event.hpp>

#include <gtest/gtest.h>

using crisp::createDelegate;
using crisp::Event;

namespace {

class EventData {
public:
    EventData(std::string msg, double x, double y)
        : message(std::move(msg))
        , x(x)
        , y(y) {}

    std::string message;
    double x;
    double y;
};

struct DelegateTester {
    std::string message;
    int state = 0;

    int triggerCounter = 0;

    void onEventTriggered(int newStateValue, std::string msg) {
        message = std::move(msg);
        state = newStateValue;
        ++triggerCounter;
    }

    void onEventData(const EventData& evData) {
        message = evData.message;
        state = static_cast<int>(evData.x * evData.y);
    }
};

void printStuff(const std::string&) {}
} // namespace

TEST(EventTest, Subscriptions) {
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

TEST(EventTest, Disconnects) {
    Event<const EventData&> event;
    DelegateTester tester;

    const auto del = createDelegate<&DelegateTester::onEventData>(&tester);

    event += del;
    event += {&tester, [](const EventData&) {}};
    EXPECT_EQ(event.getSubscriberCount(), 2);
    EXPECT_EQ(event.getDelegateCount(), 1);
    EXPECT_EQ(event.getFunctorCount(), 1);

    EventData data("Hello", 10, 20);

    event(data);

    EXPECT_EQ(tester.state, 200);

    event.unsubscribe(&tester);
    EXPECT_EQ(event.getSubscriberCount(), 0);
}

TEST(EventTest, StaticFunctions) {
    Event<const std::string&> event;

    const auto freeDelegate = createDelegate<printStuff>();
    freeDelegate("Hello World!");

    event.subscribe<printStuff>();
    EXPECT_EQ(event.getSubscriberCount(), 1);
}

TEST(EventTest, Autodisconnect) {
    Event<const std::string&> event;
    event.subscribe<&printStuff>();

    {
        const auto handler = event.subscribe([](const std::string& a) { std::cout << a.size() << '\n'; });
        EXPECT_EQ(event.getSubscriberCount(), 2);
    }

    EXPECT_EQ(event.getSubscriberCount(), 1);
}
