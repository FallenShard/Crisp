#include "gtest/gtest.h"

#include "CrispCore/CommandLineParser.hpp"

TEST(CommandLineParserTest, Basic)
{
    const char* argLine = "Program123 width 443 path some_directive height=23";

    crisp::CommandLineParser clp;
    clp.addOption<int32_t>("width", 0);
    clp.addOption<std::string>("path", "");
    clp.addOption<int16_t>("height", 0);
    clp.addOption<std::string>("find_all_enemies", "not");

    clp.parse(argLine);

    ASSERT_EQ(clp.get<int32_t>("width"), 443);
    ASSERT_EQ(clp.get<int16_t>("height"), 23);
    ASSERT_EQ(clp.get<std::string>("path"), "some_directive");
    ASSERT_EQ(clp.get<std::string>("find_all_enemies"), "not");
}
