# Test files mirror the layout of src/ecs/ (Entity/, Component/, ...).

rtype_add_test(ecs_tests
    SOURCES
        Entity/EntityTests.cpp
        Entity/EntityManagerTests.cpp
    LIBRARIES
        rtype::ecs
)
