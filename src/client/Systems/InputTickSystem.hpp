#pragma once

#include "ecs/System/System.hpp"

namespace client {

/// Simulation (60 Hz = the input rate, §9): records the keys held during this
/// tick in the `InputHistory`, while playing. NetSendSystem sends them.
class InputTickSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace client
