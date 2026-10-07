#pragma once

namespace ecs {

class World;
class Time;

/// Base class of every system.
///
/// A system contains behavior: each frame, it reads and writes components
/// through the World, usually with a query.
///
///     class MovementSystem final : public ecs::System {
///     public:
///         void update(ecs::World& world, const ecs::Time& time) override
///         {
///             world.query<Position, Velocity>(
///                 [&](ecs::Entity, Position& position, Velocity& velocity) {
///                     position.x += velocity.dx * time.deltaSeconds();
///                 });
///         }
///     };
///
/// Rules:
///  - a system has one small, clear responsibility;
///  - a system never calls another system: systems communicate through
///    components, events, commands and resources;
///  - a system knows component types, never "players" or "enemies" as classes.
class System {
public:
    virtual ~System() = default;

    System(const System&) = delete;
    System& operator=(const System&) = delete;
    System(System&&) = delete;
    System& operator=(System&&) = delete;

    /// Runs the system for one frame.
    virtual void update(World& world, const Time& time) = 0;

protected:
    System() = default;
};

} // namespace ecs
