#pragma once

#include "ecs/Event/EventChannel.hpp"

#include <memory>

namespace ecs {

/// Keeps a handler subscribed for as long as it is alive (RAII).
///
///     class DamageSystem final : public ecs::System {
///     public:
///         explicit DamageSystem(ecs::EventBus& events)
///             : _onCollision(events.subscribe<CollisionEvent>(
///                   [this](const CollisionEvent& event) { onCollision(event); }))
///         {
///         }
///
///     private:
///         ecs::Subscription _onCollision; // Unsubscribes when the system is destroyed
///     };
///
/// Destroying or resetting the subscription unsubscribes the handler.
/// It is safe even if the EventBus was destroyed first.
/// A subscription is movable but not copyable: it owns the registration.
class Subscription {
public:
    /// An empty subscription, attached to nothing.
    Subscription() noexcept = default;

    Subscription(std::weak_ptr<IEventChannel> channel, HandlerId id) noexcept;

    ~Subscription();

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept;
    Subscription& operator=(Subscription&& other) noexcept;

    /// Unsubscribes now. Does nothing if already unsubscribed.
    void reset() noexcept;

    /// True while the handler is subscribed to a living EventBus.
    [[nodiscard]]
    bool active() const noexcept;

private:
    std::weak_ptr<IEventChannel> _channel;
    HandlerId _id{0};
};

} // namespace ecs
