#include "server/Systems/InputSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"

#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

void apply(PlayerInput& player, const Input& input)
{
    for (std::uint8_t i = 0; i < input.count; ++i) {
        // shortcut: ticks are compared without wrap-around; a u32 at 60 Hz
        // wraps after 2.2 years of play
        const std::uint32_t tick = input.firstTick + i;
        if (player.received
            && (tick <= player.lastTick || tick - player.lastTick > InputSystem::MaxTicksAhead)) {
            continue; // Already applied, or an impossible jump ahead
        }
        // shortcut: the newest mask wins; replaying every tick's mask in order
        // comes with client-side prediction (#80)
        player.mask = input.masks.at(i);
        player.lastTick = tick;
        player.received = true;
    }
}

} // namespace

void InputSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::vector<std::pair<ecs::Entity, Input>> inputs;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        if (const auto* input = std::get_if<Input>(&message.payload)) {
            inputs.emplace_back(entity, *input);
        }
    });

    for (const auto& [messageEntity, input] : inputs) {
        const ecs::Entity player = world.getComponent<ClientMessage>(messageEntity).client;
        world.destroyEntity(messageEntity);
        if (!world.hasComponent<NetClient>(player)) {
            continue; // The player left during this frame
        }
        if (!world.hasComponent<PlayerInput>(player)) {
            world.addComponent(player, PlayerInput{});
        }
        apply(world.getComponent<PlayerInput>(player), input);
    }
}

} // namespace server
