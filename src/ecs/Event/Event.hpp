#pragma once

#include <concepts>

namespace ecs {

/// Requirements for a type to be used as an event.
///
/// An event is a plain struct describing something that happened:
///
///     struct CollisionEvent {
///         ecs::Entity first;
///         ecs::Entity second;
///     };
///
/// Events are copied into the EventBus queue, so they must be copyable.
/// No base class, no registration: any new struct can be emitted.
template <typename T>
concept Event = std::copyable<T>;

} // namespace ecs
