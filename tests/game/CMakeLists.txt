# Test files mirror the layout of src/game/.

rtype_add_test(game_tests
    SOURCES
        Systems/MovementSystemTests.cpp
        Systems/ConfinementSystemTests.cpp
        Systems/CollisionSystemTests.cpp
    LIBRARIES
        rtype::game
)
