# rtype_game — R-Type gameplay shared by the server and the client.
#
# Components, constants and the systems both sides must run identically (the
# server to decide, the client to predict). It knows nothing about the
# network: the server and client translate between packets and game state.
#
# Headers are included from the src/ root, e.g. "game/Components/Motion.hpp".

add_library(rtype_game
    Arena.hpp
    Ship.hpp

    Components/Motion.hpp

    Systems/MovementSystem.hpp
    Systems/MovementSystem.cpp
)
add_library(rtype::game ALIAS rtype_game)

target_include_directories(rtype_game PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_game PUBLIC cxx_std_23)
target_link_libraries(rtype_game PUBLIC rtype::ecs)
rtype_set_warnings(rtype_game)
