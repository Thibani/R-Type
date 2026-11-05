#pragma once

#include <cstddef>

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

} // namespace ecs
