#pragma once

#include "ecs/Event/Event.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace ecs {

/// Identifier of a handler inside its channel.
using HandlerId = std::size_t;

/// Type-erased view of an `EventChannel`, used by `Subscription` to
/// unsubscribe without knowing the event type.
class IEventChannel {
public:
    virtual ~IEventChannel() = default;

    IEventChannel(const IEventChannel&) = delete;
    IEventChannel& operator=(const IEventChannel&) = delete;
    IEventChannel(IEventChannel&&) = delete;
    IEventChannel& operator=(IEventChannel&&) = delete;

    virtual void removeHandler(HandlerId id) noexcept = 0;

protected:
    IEventChannel() = default;
};

/// The handlers subscribed to one event type `E`.
///
/// Handlers are called in subscription order. A handler may unsubscribe
/// itself or others while an event is being delivered: removed handlers are
/// only marked as removed and cleaned up once the delivery is over.
template <Event E>
class EventChannel final : public IEventChannel {
public:
    using Handler = std::function<void(const E&)>;

    HandlerId addHandler(Handler handler)
    {
        const HandlerId id = _nextId++;
        _handlers.push_back(Entry{.id = id, .handler = std::move(handler)});
        return id;
    }

    void removeHandler(HandlerId id) noexcept override
    {
        const auto found = std::ranges::find(_handlers, id, &Entry::id);
        if (found == _handlers.end()) {
            return;
        }
        if (_delivering) {
            found->handler = nullptr; // Erased after the delivery, see deliver()
        } else {
            _handlers.erase(found);
        }
    }

    /// Calls every handler with `event`.
    /// Handlers added during the delivery only receive the next events.
    void deliver(const E& event)
    {
        const DeliveryScope scope{*this};

        const std::size_t handlerCount = _handlers.size();
        for (std::size_t index = 0; index < handlerCount; ++index) {
            // Copied: the handler may add handlers, which can reallocate _handlers.
            const Handler handler = _handlers[index].handler;
            if (handler) {
                handler(event);
            }
        }
    }

    [[nodiscard]]
    std::size_t handlerCount() const noexcept
    {
        return static_cast<std::size_t>(std::ranges::count_if(
            _handlers, [](const Entry& entry) { return static_cast<bool>(entry.handler); }));
    }

private:
    struct Entry {
        HandlerId id;
        Handler handler;
    };

    /// Marks the channel as delivering, and cleans up the handlers removed
    /// meanwhile when the delivery ends (even if a handler throws).
    class DeliveryScope {
    public:
        explicit DeliveryScope(EventChannel& channel) noexcept
            : _channel(channel)
        {
            _channel._delivering = true;
        }

        ~DeliveryScope()
        {
            _channel._delivering = false;
            std::erase_if(_channel._handlers, [](const Entry& entry) { return !entry.handler; });
        }

        DeliveryScope(const DeliveryScope&) = delete;
        DeliveryScope& operator=(const DeliveryScope&) = delete;
        DeliveryScope(DeliveryScope&&) = delete;
        DeliveryScope& operator=(DeliveryScope&&) = delete;

    private:
        EventChannel& _channel;
    };

    std::vector<Entry> _handlers;
    HandlerId _nextId{0};
    bool _delivering{false};
};

} // namespace ecs
