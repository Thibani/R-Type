#include "ecs/Event/EventBus.hpp"
#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <vector>

using ecs::Entity;
using ecs::Time;
using ecs::World;

namespace {

struct Health {
    int current{};
};

struct Hit {
    Entity target;
    int amount{};
};

/// Emits a hit on every entity having Health, without knowing who handles it.
class HitEverythingSystem final : public ecs::System {
public:
    void update(World& world, const Time& /*time*/) override
    {
        world.query<Health>([&](Entity entity, Health&) {
            world.events().emit(Hit{.target = entity, .amount = 3});
        });
    }
};

/// Applies hits. Subscribes in its constructor, unsubscribes when destroyed.
class DamageSystem final : public ecs::System {
public:
    explicit DamageSystem(World& world)
        : _world(world)
        , _onHit(world.events().subscribe<Hit>([this](const Hit& hit) { apply(hit); }))
    {
    }

    void update(World& /*world*/, const Time& /*time*/) override { }

private:
    void apply(const Hit& hit)
    {
        if (auto* health = _world.get().tryGetComponent<Health>(hit.target)) {
            health->current -= hit.amount;
        }
    }

    std::reference_wrapper<World> _world;
    ecs::Subscription _onHit;
};

} // namespace

TEST(WorldEvents, EventsEmittedBySystemsAreDeliveredAtTheEndOfUpdate)
{
    World world;
    world.systems().add<HitEverythingSystem>();
    world.systems().add<DamageSystem>(world);
    const Entity target = world.createEntity();
    world.addComponent(target, Health{.current = 10});

    world.update(Time{});

    EXPECT_EQ(world.getComponent<Health>(target).current, 7);
    EXPECT_EQ(world.events().pendingCount(), 0U);
}

TEST(WorldEvents, RemovingASystemRemovesItsSubscriptions)
{
    World world;
    world.systems().add<DamageSystem>(world);
    EXPECT_EQ(world.events().subscriberCount<Hit>(), 1U);

    world.systems().remove<DamageSystem>();

    EXPECT_EQ(world.events().subscriberCount<Hit>(), 0U);
}

TEST(WorldEvents, ClearDropsPendingEvents)
{
    World world;
    world.systems().add<DamageSystem>(world);
    world.events().emit(Hit{});

    world.clear();

    EXPECT_EQ(world.events().pendingCount(), 0U);
    EXPECT_EQ(world.events().subscriberCount<Hit>(), 1U);
}
