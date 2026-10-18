# rtype_game — R-Type gameplay shared by the server and the client.
#
# Components and constants (plain data) plus the gameplay systems that act on
# them. The server runs the systems (it is authoritative); the client can run
# the same ones to predict, and displays the components.
#
# Headers are included from the src/ root, e.g. "game/Components/Motion.hpp".

add_library(rtype_game
    Arena.hpp

    Components/Motion.hpp
    Components/Confinement.hpp

    Systems/MovementSystem.hpp
    Systems/MovementSystem.cpp
    Systems/ConfinementSystem.hpp
    Systems/ConfinementSystem.cpp
)
add_library(rtype::game ALIAS rtype_game)

target_include_directories(rtype_game PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_game PUBLIC cxx_std_23)
target_link_libraries(rtype_game PUBLIC rtype::ecs)
rtype_set_warnings(rtype_game)
