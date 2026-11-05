# r-type_server — the authoritative game server.
#
# rtype_server holds the server's components and systems, so the tests can
# link them; the executable only adds main.cpp on top.
#
# Headers are included from the src/ root, e.g. "server/ServerOptions.hpp".

add_library(rtype_server
    ServerOptions.hpp
    ServerOptions.cpp

    Network.hpp
    Network.cpp
    NetIds.hpp
    NetIds.cpp
    Singleton.hpp
    ServerSystems.hpp
    ServerSystems.cpp

    Components/ReceivedPacket.hpp
    Components/NetClient.hpp
    Components/ClientMessage.hpp
    Components/Player.hpp
    Components/NetEntity.hpp
    Components/Combat.hpp
    Components/Behaviour.hpp
    Components/Waves.hpp
    Components/Match.hpp
    Spawn.hpp
    Spawn.cpp
    Run.hpp
    Run.cpp

    Systems/NetReceiveSystem.hpp
    Systems/NetReceiveSystem.cpp
    Systems/ConnectionSystem.hpp
    Systems/ConnectionSystem.cpp
    Systems/SessionReceiveSystem.hpp
    Systems/SessionReceiveSystem.cpp
    Systems/TimeoutSystem.hpp
    Systems/TimeoutSystem.cpp
    Systems/SessionCleanupSystem.hpp
    Systems/SessionCleanupSystem.cpp
    Systems/NetSendSystem.hpp
    Systems/NetSendSystem.cpp
    Systems/InputSystem.hpp
    Systems/InputSystem.cpp
    Systems/RestartSystem.hpp
    Systems/RestartSystem.cpp
    Systems/PlayerShipSystem.hpp
    Systems/PlayerShipSystem.cpp
    Systems/TickSystem.hpp
    Systems/TickSystem.cpp
    Systems/MatchSystem.hpp
    Systems/MatchSystem.cpp
    Systems/PlayerControlSystem.hpp
    Systems/PlayerControlSystem.cpp
    Systems/ProgressSystem.hpp
    Systems/ProgressSystem.cpp
    Systems/WaveSystem.hpp
    Systems/WaveSystem.cpp
    Systems/WaveFlightSystem.hpp
    Systems/WaveFlightSystem.cpp
    Systems/SeekerSystem.hpp
    Systems/SeekerSystem.cpp
    Systems/AimSystem.hpp
    Systems/AimSystem.cpp
    Systems/CircleFlightSystem.hpp
    Systems/CircleFlightSystem.cpp
    Systems/HopSystem.hpp
    Systems/HopSystem.cpp
    Systems/HoverSystem.hpp
    Systems/HoverSystem.cpp
    Systems/BossSystem.hpp
    Systems/BossSystem.cpp
    Systems/DeathBurstSystem.hpp
    Systems/DeathBurstSystem.cpp
    Systems/WeaponSystem.hpp
    Systems/WeaponSystem.cpp
    Systems/DamageSystem.hpp
    Systems/DamageSystem.cpp
    Systems/LivesSystem.hpp
    Systems/LivesSystem.cpp
    Systems/ReplicationSystem.hpp
    Systems/ReplicationSystem.cpp
    Systems/SnapshotSystem.hpp
    Systems/SnapshotSystem.cpp
    Systems/DiscardPacketsSystem.hpp
    Systems/DiscardPacketsSystem.cpp
)
add_library(rtype::server ALIAS rtype_server)

target_include_directories(rtype_server PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_server PUBLIC cxx_std_23)
target_link_libraries(rtype_server PUBLIC rtype::ecs rtype::network rtype::game)
rtype_set_warnings(rtype_server)

add_executable(r-type_server main.cpp)
target_link_libraries(r-type_server PRIVATE rtype::server)
rtype_set_warnings(r-type_server)
# Next to the build folder's root (build/<preset>/r-type_server), easy to find
set_target_properties(r-type_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
