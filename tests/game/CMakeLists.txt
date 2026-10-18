# Test files mirror the layout of src/game/.

rtype_add_test(game_tests
    SOURCES
        Systems/MovementSystemTests.cpp
        Systems/ConfinementSystemTests.cpp
        Systems/CollisionSystemTests.cpp
        Systems/LifetimeSystemTests.cpp
        Systems/ArenaExitSystemTests.cpp
        Systems/DespawnSystemTests.cpp
    LIBRARIES
        rtype::game
)
