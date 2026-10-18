#pragma once

#include "ecs/Event/Event.hpp"
#include "ecs/Event/EventChannel.hpp"
#include "ecs/Event/Subscription.hpp"

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ecs {

/// Lets systems communicate without knowing each other.
///
/// A system that detects something emits an event; every system interested
/// in it subscribes. The emitter never knows who listens:
///
///     // CollisionSystem
///     events.emit(CollisionEvent{.first = a, .second = b});
///
///     // DamageSystem, AudioSystem, NetworkSystem...
///     _subscription = events.subscribe<CollisionEvent>([](const CollisionEvent& event) { ... });
///
/// Events are QUEUED, not delivered immediately: `emit()` only stores them,
/// `dispatch()` delivers them all, in emission order. This keeps delivery at a
/// predictable point of the frame (never in the middle of a system's query).
/// `World::update()` dispatches after running the systems.
///
/// Events emitted by handlers during `dispatch()` are delivered in the same
/// `dispatch()`, after the events already queued, up to `MaxDispatchRounds`
/// rounds. What is left (an endless chain of events) waits for the next call.
class EventBus {
public:
    /// Rounds of "handlers emit new events" handled by one `dispatch()`.
    static constexpr std::size_t MaxDispatchRounds = 16;

    EventBus() = default;
    ~EventBus() = default;

    // Subscriptions point to this bus's channels: moving it would be confusing.
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    /// Subscribes `handler` to the events of type `E`.
    /// The handler stays subscribed as long as the returned Subscription lives.
    template <Event E, typename Handler>
        requires std::invocable<Handler&, const E&>
    [[nodiscard]]
    Subscription subscribe(Handler handler)
    {
        const std::shared_ptr<EventChannel<E>> typedChannel = channel<E>();
        const HandlerId id = typedChannel->addHandler(std::move(handler));
        return Subscription{typedChannel, id};
    }

    /// Queues `event`. It is delivered by the next `dispatch()`.
    /// An event of a type nobody subscribed to is simply dropped.
    template <Event E>
    void emit(E event)
    {
        const std::shared_ptr<EventChannel<E>> typedChannel = findChannel<E>();
        if (typedChannel == nullptr) {
            return; // Nobody listens: nothing to deliver
        }
        _queue.emplace_back(
            [typedChannel, queuedEvent = std::move(event)] { typedChannel->deliver(queuedEvent); });
    }

    /// Delivers every queued event, in emission order.
    /// Calling it from inside a handler does nothing (dispatch is not re-entrant).
    void dispatch();

    /// Number of events waiting for the next `dispatch()`.
    [[nodiscard]]
    std::size_t pendingCount() const noexcept;

    /// Number of handlers currently subscribed to `E`.
    template <Event E>
    [[nodiscard]]
    std::size_t subscriberCount() const noexcept
    {
        const std::shared_ptr<EventChannel<E>> typedChannel = findChannel<E>();
        return typedChannel != nullptr ? typedChannel->handlerCount() : 0;
    }

    /// Drops every queued event. Subscriptions are kept.
    void clear() noexcept;

private:
    using Delivery = std::function<void()>;

    template <Event E>
    [[nodiscard]]
    std::shared_ptr<EventChannel<E>> channel()
    {
        if (std::shared_ptr<EventChannel<E>> existing = findChannel<E>()) {
            return existing;
        }
        auto created = std::make_shared<EventChannel<E>>();
        _channels.emplace(std::type_index(typeid(E)), created);
        return created;
    }

    template <Event E>
    [[nodiscard]]
    std::shared_ptr<EventChannel<E>> findChannel() const
    {
        const auto found = _channels.find(std::type_index(typeid(E)));
        if (found == _channels.end()) {
            return nullptr;
        }
        // Safe: the channel registered under E's type is always an EventChannel<E>.
        return std::static_pointer_cast<EventChannel<E>>(found->second);
    }

    // shared_ptr: a Subscription keeps a weak_ptr to its channel, so it can
    // outlive the bus safely.
    std::unordered_map<std::type_index, std::shared_ptr<IEventChannel>> _channels;
    std::vector<Delivery> _queue;
    bool _dispatching{false};
};

} // namespace ecs
