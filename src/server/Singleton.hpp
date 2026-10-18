#pragma once

#include "ecs/Component/Component.hpp"
#include "ecs/World/World.hpp"

namespace server {

/// The one `T` of the world, created on first use: state shared by several
/// systems (the tick counter, the netId counter...) lives on its own entity,
/// like any other data, instead of in a system or a global.
template <ecs::Component T>
T& singleton(ecs::World& world)
{
    T* found = nullptr;
    world.query<T>([&](ecs::Entity /*entity*/, T& component) { found = &component; });
    if (found != nullptr) {
        return *found;
    }
    return world.addComponent(world.createEntity(), T{});
}

} // namespace server
