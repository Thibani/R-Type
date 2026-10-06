# Test files mirror the layout of src/ecs/ (Entity/, Component/, ...).

rtype_add_test(ecs_tests
    SOURCES
        Entity/EntityTests.cpp
        Entity/EntityManagerTests.cpp
        Component/ComponentTests.cpp
        Component/ComponentStorageTests.cpp
        Component/ComponentManagerTests.cpp
        World/WorldTests.cpp
        World/QueryTests.cpp
    LIBRARIES
        rtype::ecs
)
