# User-facing build options.
# Override them on the command line, e.g. -DRTYPE_BUILD_TESTS=OFF

option(RTYPE_BUILD_TESTS "Build the unit and integration tests" ON)
option(RTYPE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()
