#include "client/ClientOptions.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace {

std::expected<client::ClientOptions, std::string> parse(std::vector<std::string_view> arguments)
{
    return client::parseArguments(arguments);
}

} // namespace

TEST(ClientOptions, DefaultsWithoutArguments)
{
    const auto options = parse({});
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->host, "127.0.0.1");
    EXPECT_EQ(options->port, 4242);
    EXPECT_EQ(options->name, "Player");
    EXPECT_FALSE(options->help);
}

TEST(ClientOptions, ReadsServerPortAndName)
{
    const auto options = parse({"-s", "10.0.0.2", "-p", "5000", "-n", "Thibani"});
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->host, "10.0.0.2");
    EXPECT_EQ(options->port, 5000);
    EXPECT_EQ(options->name, "Thibani");
}

TEST(ClientOptions, LongFormsAndHelp)
{
    const auto options
        = parse({"--server", "localhost", "--port", "4243", "--name", "A", "--help"});
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->host, "localhost");
    EXPECT_EQ(options->port, 4243);
    EXPECT_TRUE(options->help);
}

TEST(ClientOptions, RejectsInvalidValues)
{
    EXPECT_FALSE(parse({"-p", "0"}).has_value());
    EXPECT_FALSE(parse({"-p", "65536"}).has_value());
    EXPECT_FALSE(parse({"-p", "42a"}).has_value());
    EXPECT_FALSE(parse({"-n", ""}).has_value());
    EXPECT_FALSE(parse({"-n", "a name longer than 16"}).has_value());
    EXPECT_FALSE(parse({"-s"}).has_value()); // Missing value
    EXPECT_FALSE(parse({"--unknown"}).has_value());
}
