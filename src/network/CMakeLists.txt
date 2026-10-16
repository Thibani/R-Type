# rtype_network — binary protocol, UDP transport and reliability.
#
# Depends only on the C++ standard library and Asio (Transport/). It knows
# nothing about the ECS: the server and client translate between packets and
# game state.
#
# Headers are included from the src/ root, e.g. "network/Protocol/Packets.hpp".

add_library(rtype_network
    Serialization/ByteReader.hpp
    Serialization/ByteWriter.hpp

    Protocol/Protocol.hpp
    Protocol/Packets.hpp
    Protocol/PacketCodec.hpp
    Protocol/PacketCodec.cpp

    Transport/UdpSocket.hpp
    Transport/UdpSocket.cpp
    Transport/NetworkThread.hpp
    Transport/NetworkThread.cpp

    Reliability/ReliableChannel.hpp
    Reliability/ReliableChannel.cpp
)
add_library(rtype::network ALIAS rtype_network)

target_include_directories(rtype_network PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(rtype_network PUBLIC cxx_std_23)
target_link_libraries(rtype_network PUBLIC asio::asio) # Public: Endpoint is an Asio type
rtype_set_warnings(rtype_network)
