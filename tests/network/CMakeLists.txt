# Test files mirror the layout of src/network/ (Serialization/, Protocol/, Transport/, Reliability/, Crypto/).

rtype_add_test(network_tests
    SOURCES
        Serialization/ByteReaderTests.cpp
        Serialization/ByteWriterTests.cpp
        Protocol/ProtocolTests.cpp
        Protocol/PacketCodecTests.cpp
        Protocol/PacketCodecRobustnessTests.cpp
        Transport/UdpSocketTests.cpp
        Transport/NetworkThreadTests.cpp
        Reliability/ReliableChannelTests.cpp
        Crypto/CryptoTests.cpp
        Crypto/SealedDatagramTests.cpp
    LIBRARIES
        rtype::network
)
