#include "server/ServerOptions.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string_view>
#include <vector>

using server::parseArguments;

namespace {

auto parse(std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> list(arguments);
    return parseArguments(list);
}

} // namespace

TEST(ServerOptions, NoArgumentUsesTheDefaultPort)
{
    const auto options = parse({});

    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->port, 4242);
    EXPECT_FALSE(options->help);
}

TEST(ServerOptions, PortCanBeChosen)
{
    EXPECT_EQ(parse({"-p", "5000"})->port, 5000);
    EXPECT_EQ(parse({"--port", "1"})->port, 1);
    EXPECT_EQ(parse({"-p", "65535"})->port, 65535);
}

TEST(ServerOptions, HelpIsRecognised)
{
    EXPECT_TRUE(parse({"-h"})->help);
    EXPECT_TRUE(parse({"--help"})->help);
}

TEST(ServerOptions, InvalidPortsAreRejected)
{
    for (const std::string_view port : {"0", "65536", "-1", "abc", "42abc", "", " 42"}) {
        SCOPED_TRACE(port);
        EXPECT_FALSE(parse({"-p", port}).has_value());
    }
}

TEST(ServerOptions, MissingPortIsRejected)
{
    const auto options = parse({"-p"});

    ASSERT_FALSE(options.has_value());
    EXPECT_NE(options.error().find("needs a port"), std::string::npos);
}

TEST(ServerOptions, UnknownArgumentIsRejected)
{
    const auto options = parse({"--verbose"});

    ASSERT_FALSE(options.has_value());
    EXPECT_NE(options.error().find("--verbose"), std::string::npos);
}
