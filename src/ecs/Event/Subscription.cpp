#include "ecs/Event/Subscription.hpp"

#include <utility>

namespace ecs {

Subscription::Subscription(std::weak_ptr<IEventChannel> channel, HandlerId id) noexcept
    : _channel(std::move(channel))
    , _id(id)
{
}

Subscription::~Subscription()
{
    reset();
}

Subscription::Subscription(Subscription&& other) noexcept
    : _channel(std::exchange(other._channel, {}))
    , _id(other._id)
{
}

Subscription& Subscription::operator=(Subscription&& other) noexcept
{
    if (this != &other) {
        reset();
        _channel = std::exchange(other._channel, {});
        _id = other._id;
    }
    return *this;
}

void Subscription::reset() noexcept
{
    if (const std::shared_ptr<IEventChannel> channel = _channel.lock()) {
        channel->removeHandler(_id);
    }
    _channel.reset();
}

bool Subscription::active() const noexcept
{
    return !_channel.expired();
}

} // namespace ecs
