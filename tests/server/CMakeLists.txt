# Test files mirror the layout of src/server/.

rtype_add_test(server_tests
    SOURCES
        ServerOptionsTests.cpp
        NetReceiveSystemTests.cpp
        SessionTests.cpp
        GameStateTests.cpp
        PlayerMovementTests.cpp
    LIBRARIES
        rtype::server
)

# The executable itself: exit codes of the command line
add_test(NAME r-type_server.help COMMAND r-type_server --help)
add_test(NAME r-type_server.invalid-arguments COMMAND r-type_server --port 0)
set_tests_properties(r-type_server.invalid-arguments PROPERTIES
    PASS_REGULAR_EXPRESSION "invalid port") # Exits with 84: the message is what proves it failed for the right reason
