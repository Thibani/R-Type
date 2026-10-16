#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Destroys the `ReceivedPacket` entities no system handled this frame, so
/// they never pile up. Runs last in `frameEnd()`.
class DiscardPacketsSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
