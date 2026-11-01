# r-type_client — shows the game and sends the player's keys to the server.
#
# rtype_client holds the client's components and systems, so the tests can
# link them; the executable only adds main.cpp on top.
#
# Headers are included from the src/ root, e.g. "client/ClientOptions.hpp".

# SFML 3 (graphics, window, audio), fetched by CPM like the other dependencies.
# SFML_USE_SYSTEM_DEPS OFF: on Linux SFML would otherwise look for system
# freetype / vorbis / flac; SFML fetches them itself, so nothing third-party
# has to be installed (only X11, OpenGL and udev headers, see README).
CPMAddPackage(
    NAME SFML
    GITHUB_REPOSITORY SFML/SFML
    GIT_TAG 3.1.0
    OPTIONS
        "SFML_BUILD_NETWORK OFF"
        "SFML_USE_SYSTEM_DEPS OFF"
        "BUILD_SHARED_LIBS OFF")

add_library(rtype_client
    ClientOptions.hpp
    ClientOptions.cpp
    Settings.hpp
    Settings.cpp
    Singleton.hpp
    Replication.hpp
    Replication.cpp
    Assets.hpp
    Assets.cpp
    ClientSystems.hpp
    ClientSystems.cpp

    Network/ServerConnection.hpp
    Network/ServerConnection.cpp

    Components/NetEntity.hpp
    Components/Appearance.hpp
    Components/Session.hpp
    Components/Controls.hpp
    Components/SoundEvent.hpp
    Components/Settings.hpp

    Systems/InputSystem.hpp
    Systems/InputSystem.cpp
    Systems/NetReceiveSystem.hpp
    Systems/NetReceiveSystem.cpp
    Systems/MotionSystem.hpp
    Systems/MotionSystem.cpp
    Systems/StarfieldSystem.hpp
    Systems/StarfieldSystem.cpp
    Systems/InputTickSystem.hpp
    Systems/InputTickSystem.cpp
    Systems/NetSendSystem.hpp
    Systems/NetSendSystem.cpp
    Systems/SettingsSystem.hpp
    Systems/SettingsSystem.cpp
    Systems/AudioSystem.hpp
    Systems/AudioSystem.cpp
    Systems/RenderSystem.hpp
    Systems/RenderSystem.cpp
    Systems/InterfaceSystem.hpp
    Systems/InterfaceSystem.cpp

    Ui/Theme.hpp
    Ui/Layout.hpp
    Ui/View.hpp
    Ui/Widgets.hpp
    Ui/Widgets.cpp
)
add_library(rtype::client ALIAS rtype_client)

target_include_directories(rtype_client PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_client PUBLIC cxx_std_23)
target_link_libraries(rtype_client
    PUBLIC rtype::ecs rtype::network rtype::game SFML::Graphics SFML::Audio)
rtype_set_warnings(rtype_client)

add_executable(r-type_client main.cpp)
target_link_libraries(r-type_client PRIVATE rtype::client)
rtype_set_warnings(r-type_client)
# Next to the build folder's root (build/<preset>/r-type_client), like the server
set_target_properties(r-type_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

# Sprites, fonts and sounds next to the executable, where it looks for them
add_custom_command(TARGET r-type_client POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets" "${PROJECT_BINARY_DIR}/assets"
    COMMENT "Copying the client assets")
