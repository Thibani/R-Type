#include "ecs/Event/EventBus.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using ecs::EventBus;
using ecs::Subscription;

namespace {

struct DamageEvent {
    int amount{};
};

struct DeathEvent {
    int id{};
};

} // namespace

TEST(EventBus, DeliversEventsOnDispatch)
{
    EventBus events;
    int received = 0;
    const Subscription subscription = events.subscribe<DamageEvent>(
        [&](const DamageEvent& event) { received += event.amount; });

    events.emit(DamageEvent{.amount = 10});

    EXPECT_EQ(received, 0); // Queued, not delivered yet
    EXPECT_EQ(events.pendingCount(), 1U);

    events.dispatch();

    EXPECT_EQ(received, 10);
    EXPECT_EQ(events.pendingCount(), 0U);
}

TEST(EventBus, EveryEventIsDeliveredOnlyOnce)
{
    EventBus events;
    int calls = 0;
    const Subscription subscription
        = events.subscribe<DamageEvent>([&](const DamageEvent&) { ++calls; });

    events.emit(DamageEvent{});
    events.dispatch();
    events.dispatch();

    EXPECT_EQ(calls, 1);
}

TEST(EventBus, EverySubscriberReceivesTheEvent)
{
    EventBus events;
    std::vector<std::string> log;
    const Subscription damage
        = events.subscribe<DamageEvent>([&](const DamageEvent&) { log.emplace_back("damage"); });
    const Subscription audio
        = events.subscribe<DamageEvent>([&](const DamageEvent&) { log.emplace_back("audio"); });

    events.emit(DamageEvent{});
    events.dispatch();

    EXPECT_EQ(log, (std::vector<std::string>{"damage", "audio"}));
}

TEST(EventBus, EventTypesAreIndependent)
{
    EventBus events;
    int damages = 0;
    int deaths = 0;
    const Subscription onDamage
        = events.subscribe<DamageEvent>([&](const DamageEvent&) { ++damages; });
    const Subscription onDeath = events.subscribe<DeathEvent>([&](const DeathEvent&) { ++deaths; });

    events.emit(DamageEvent{});
    events.emit(DamageEvent{});
    events.emit(DeathEvent{});
    events.dispatch();

    EXPECT_EQ(damages, 2);
    EXPECT_EQ(deaths, 1);
}

TEST(EventBus, EventsAreDeliveredInEmissionOrderAcrossTypes)
{
    EventBus events;
    std::vector<std::string> log;
    const Subscription onDamage = events.subscribe<DamageEvent>(
        [&](const DamageEvent& event) { log.push_back("damage " + std::to_string(event.amount)); });
    const Subscription onDeath = events.subscribe<DeathEvent>(
        [&](const DeathEvent& event) { log.push_back("death " + std::to_string(event.id)); });

    events.emit(DamageEvent{.amount = 1});
    events.emit(DeathEvent{.id = 2});
    events.emit(DamageEvent{.amount = 3});
    events.dispatch();

    EXPECT_EQ(log, (std::vector<std::string>{"damage 1", "death 2", "damage 3"}));
}

TEST(EventBus, EventWithoutSubscriberIsDropped)
{
    EventBus events;

    events.emit(DamageEvent{});

    EXPECT_EQ(events.pendingCount(), 0U);
}

TEST(EventBus, EventsEmittedByHandlersAreDeliveredInTheSameDispatch)
{
    EventBus events;
    std::vector<std::string> log;
    const Subscription onDamage = events.subscribe<DamageEvent>([&](const DamageEvent& event) {
        log.emplace_back("damage");
        if (event.amount >= 100) {
            events.emit(DeathEvent{.id = 7}); // Chain: damage -> death
        }
    });
    const Subscription onDeath
        = events.subscribe<DeathEvent>([&](const DeathEvent&) { log.emplace_back("death"); });

    events.emit(DamageEvent{.amount = 100});
    events.emit(DamageEvent{.amount = 1});
    events.dispatch();

    // The chained event comes after the events that were already queued.
    EXPECT_EQ(log, (std::vector<std::string>{"damage", "damage", "death"}));
}

TEST(EventBus, EndlessChainsAreBoundedPerDispatch)
{
    EventBus events;
    int calls = 0;
    const Subscription subscription = events.subscribe<DamageEvent>([&](const DamageEvent&) {
        ++calls;
        events.emit(DamageEvent{}); // Re-emits forever
    });

    events.emit(DamageEvent{});
    events.dispatch();

    EXPECT_EQ(calls, static_cast<int>(EventBus::MaxDispatchRounds));
    EXPECT_EQ(events.pendingCount(), 1U); // Left for the next dispatch
}

TEST(EventBus, DispatchIsNotReentrant)
{
    EventBus events;
    int deaths = 0;
    const Subscription onDeath = events.subscribe<DeathEvent>([&](const DeathEvent&) { ++deaths; });
    const Subscription onDamage = events.subscribe<DamageEvent>([&](const DamageEvent&) {
        events.emit(DeathEvent{});
        events.dispatch(); // Ignored: the outer dispatch delivers the death
        EXPECT_EQ(deaths, 0);
    });

    events.emit(DamageEvent{});
    events.dispatch();

    EXPECT_EQ(deaths, 1);
}

TEST(EventBus, ClearDropsPendingEventsButKeepsSubscriptions)
{
    EventBus events;
    int calls = 0;
    const Subscription subscription
        = events.subscribe<DamageEvent>([&](const DamageEvent&) { ++calls; });
    events.emit(DamageEvent{});

    events.clear();
    events.dispatch();
    EXPECT_EQ(calls, 0);

    events.emit(DamageEvent{});
    events.dispatch();
    EXPECT_EQ(calls, 1);
}

TEST(EventBus, EventsAreCopiedWhenEmitted)
{
    EventBus events;
    int received = 0;
    const Subscription subscription
        = events.subscribe<DamageEvent>([&](const DamageEvent& event) { received = event.amount; });

    DamageEvent event{.amount = 5};
    events.emit(event);
    event.amount = 99; // Changing the original after emit has no effect

    events.dispatch();
    EXPECT_EQ(received, 5);
}
