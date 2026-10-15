# r-type_server — the authoritative game server.
#
# rtype_server holds the server's components and systems, so the tests can
# link them; the executable only adds main.cpp on top.
#
# Headers are included from the src/ root, e.g. "server/ServerOptions.hpp".

add_library(rtype_server
    ServerOptions.hpp
    ServerOptions.cpp

    Components/ReceivedPacket.hpp

    Systems/NetReceiveSystem.hpp
    Systems/NetReceiveSystem.cpp
    Systems/DiscardPacketsSystem.hpp
    Systems/DiscardPacketsSystem.cpp
)
add_library(rtype::server ALIAS rtype_server)

target_include_directories(rtype_server PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_server PUBLIC cxx_std_23)
target_link_libraries(rtype_server PUBLIC rtype::ecs rtype::network)
rtype_set_warnings(rtype_server)

add_executable(r-type_server main.cpp)
target_link_libraries(r-type_server PRIVATE rtype::server)
rtype_set_warnings(r-type_server)
# Next to the build folder's root (build/<preset>/r-type_server), easy to find
set_target_properties(r-type_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
