#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

namespace ecs {
class GameLoop;
class World;
} // namespace ecs

namespace client {

struct Assets;
class ServerConnection;

/// Adds every client system to the loop, in order (see README.md):
///
///   frameStart  InputSystem, NetReceiveSystem
///   simulation  MotionSystem, StarfieldSystem, InputTickSystem   (60 Hz)
///   frameEnd    NetSendSystem, AudioSystem, RenderSystem, InterfaceSystem
///
/// The window, the connection and the assets must outlive the loop.
void installSystems(ecs::World& world, ecs::GameLoop& loop, sf::RenderWindow& window,
    ServerConnection& connection, const Assets& assets);

} // namespace client
