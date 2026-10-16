# rtype_game — R-Type gameplay data shared by the server and the client.
#
# Header-only for now: components and constants, no behaviour. The server's
# systems decide with them; the client's systems display them.
#
# Headers are included from the src/ root, e.g. "game/Components/Motion.hpp".

add_library(rtype_game INTERFACE)
add_library(rtype::game ALIAS rtype_game)

target_include_directories(rtype_game INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_game INTERFACE cxx_std_23)
