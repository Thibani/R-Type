# rtype_ecs — the generic Entity-Component-System.
#
# This library must stay independent from R-Type, rendering, audio and
# networking: it only depends on the C++ standard library.
#
# Each ECS concept lives in its own sub-folder (Entity/, Component/, ...).
# Headers are included from the src/ root, e.g. "ecs/Entity/Entity.hpp".

add_library(rtype_ecs
    Entity/Entity.hpp
    Entity/EntityManager.hpp
    Entity/EntityManager.cpp
)
add_library(rtype::ecs ALIAS rtype_ecs)

target_include_directories(rtype_ecs PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_ecs PUBLIC cxx_std_23)
rtype_set_warnings(rtype_ecs)
