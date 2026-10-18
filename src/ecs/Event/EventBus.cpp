#include "ecs/Event/EventBus.hpp"

namespace ecs {

namespace {

/// Sets a flag for the duration of a scope, and clears it even if a
/// handler throws.
class FlagScope {
public:
    explicit FlagScope(bool& flag) noexcept
        : _flag(flag)
    {
        _flag = true;
    }

    ~FlagScope()
    {
        _flag = false;
    }

    FlagScope(const FlagScope&) = delete;
    FlagScope& operator=(const FlagScope&) = delete;
    FlagScope(FlagScope&&) = delete;
    FlagScope& operator=(FlagScope&&) = delete;

private:
    bool& _flag;
};

} // namespace

void EventBus::dispatch()
{
    if (_dispatching) {
        return;
    }
    const FlagScope dispatching{_dispatching};

    for (std::size_t round = 0; round < MaxDispatchRounds && !_queue.empty(); ++round) {
        // Take the current batch: events emitted by handlers go to a fresh
        // queue and are delivered in the next round.
        const std::vector<Delivery> batch = std::exchange(_queue, {});
        for (const Delivery& deliver : batch) {
            deliver();
        }
    }
}

std::size_t EventBus::pendingCount() const noexcept
{
    return _queue.size();
}

void EventBus::clear() noexcept
{
    _queue.clear();
}

} // namespace ecs
