#include "ecs/Event/EventBus.hpp"
#include "ecs/Event/Subscription.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>

using ecs::EventBus;
using ecs::Subscription;

namespace {

struct PingEvent { };

} // namespace

TEST(Subscription, DestroyingTheSubscriptionUnsubscribes)
{
    EventBus events;
    int calls = 0;
    {
        const Subscription subscription
            = events.subscribe<PingEvent>([&](const PingEvent&) { ++calls; });
        EXPECT_EQ(events.subscriberCount<PingEvent>(), 1U);
    }

    EXPECT_EQ(events.subscriberCount<PingEvent>(), 0U);
    events.emit(PingEvent{});
    events.dispatch();
    EXPECT_EQ(calls, 0);
}

TEST(Subscription, ResetUnsubscribes)
{
    EventBus events;
    int calls = 0;
    Subscription subscription = events.subscribe<PingEvent>([&](const PingEvent&) { ++calls; });

    EXPECT_TRUE(subscription.active());
    subscription.reset();
    EXPECT_FALSE(subscription.active());

    events.emit(PingEvent{});
    events.dispatch();
    EXPECT_EQ(calls, 0);

    subscription.reset(); // Resetting twice is harmless
}

TEST(Subscription, EventEmittedBeforeUnsubscribingIsNotDeliveredAfter)
{
    EventBus events;
    int calls = 0;
    Subscription subscription = events.subscribe<PingEvent>([&](const PingEvent&) { ++calls; });

    events.emit(PingEvent{});
    subscription.reset();
    events.dispatch();

    EXPECT_EQ(calls, 0);
}

TEST(Subscription, MovingTransfersTheRegistration)
{
    EventBus events;
    int calls = 0;
    Subscription original = events.subscribe<PingEvent>([&](const PingEvent&) { ++calls; });

    const Subscription moved = std::move(original);

    EXPECT_FALSE(original.active()); // NOLINT(bugprone-use-after-move): checks the moved-from state
    EXPECT_TRUE(moved.active());
    events.emit(PingEvent{});
    events.dispatch();
    EXPECT_EQ(calls, 1);
}

TEST(Subscription, MoveAssignmentReleasesThePreviousRegistration)
{
    EventBus events;
    int firstCalls = 0;
    int secondCalls = 0;
    Subscription subscription
        = events.subscribe<PingEvent>([&](const PingEvent&) { ++firstCalls; });

    subscription = events.subscribe<PingEvent>([&](const PingEvent&) { ++secondCalls; });

    events.emit(PingEvent{});
    events.dispatch();
    EXPECT_EQ(firstCalls, 0);
    EXPECT_EQ(secondCalls, 1);
    EXPECT_EQ(events.subscriberCount<PingEvent>(), 1U);
}

TEST(Subscription, HandlerCanUnsubscribeItselfDuringDelivery)
{
    EventBus events;
    int calls = 0;
    Subscription subscription;
    subscription = events.subscribe<PingEvent>([&](const PingEvent&) {
        ++calls;
        subscription.reset(); // One-shot handler
    });

    events.emit(PingEvent{});
    events.emit(PingEvent{});
    events.dispatch();

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(events.subscriberCount<PingEvent>(), 0U);
}

TEST(Subscription, HandlerCanUnsubscribeAnotherDuringDelivery)
{
    EventBus events;
    int secondCalls = 0;
    Subscription second;
    const Subscription first
        = events.subscribe<PingEvent>([&](const PingEvent&) { second.reset(); });
    second = events.subscribe<PingEvent>([&](const PingEvent&) { ++secondCalls; });

    events.emit(PingEvent{});
    events.dispatch();

    EXPECT_EQ(secondCalls, 0); // Removed before its turn: not called
}

TEST(Subscription, SubscriptionCanOutliveTheBus)
{
    std::optional<Subscription> subscription;
    {
        EventBus events;
        subscription = events.subscribe<PingEvent>([](const PingEvent&) {});
        EXPECT_TRUE(subscription->active());
    }

    EXPECT_FALSE(subscription->active());
    subscription->reset(); // Must not touch the destroyed bus
}

TEST(Subscription, DefaultSubscriptionIsInactive)
{
    Subscription subscription;

    EXPECT_FALSE(subscription.active());
    subscription.reset();
}
