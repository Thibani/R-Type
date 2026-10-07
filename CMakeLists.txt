cmake_minimum_required(VERSION 3.21)

project(RType
    VERSION 0.1.0
    DESCRIPTION "Networked R-Type built on a reusable ECS game engine"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use: cmake -S . -B build")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(ProjectOptions)
include(CompilerWarnings)
include(Dependencies)

add_subdirectory(src)

if(RTYPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
