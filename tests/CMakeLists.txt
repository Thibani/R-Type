include(GoogleTest)

# rtype_add_test(<name> SOURCES <files...> LIBRARIES <targets...>)
#
# Declares a test executable linked with GoogleTest and registers every
# TEST() it contains with CTest.
function(rtype_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES} GTest::gtest_main)
    rtype_set_warnings(${name})
    gtest_discover_tests(${name})
endfunction()

add_subdirectory(ecs)
