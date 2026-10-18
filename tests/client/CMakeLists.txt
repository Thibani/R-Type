# Test files mirror the layout of src/client/. None of them opens a window.

rtype_add_test(client_tests
    SOURCES
        ClientOptionsTests.cpp
        ReplicationTests.cpp
        ServerConnectionTests.cpp
    LIBRARIES
        rtype::client
)

# The executable itself: exit codes of the command line
add_test(NAME r-type_client.help COMMAND r-type_client --help)
add_test(NAME r-type_client.invalid-arguments COMMAND r-type_client --port 0)
set_tests_properties(r-type_client.invalid-arguments PROPERTIES
    PASS_REGULAR_EXPRESSION "invalid port") # Exits with 84: the message is what proves it failed for the right reason
