#include "gtest/gtest.h"

#include "Crisp/Core/CommandLineParser.hpp"

TEST(CommandLineParserTest, Basic)
{
    const char* argLine = "Program123 width 443 --path some_directive height=23";

    int32_t width = 0;
    int16_t height = 0;
    std::string path = "";
    std::string findAllEnemies = "not";

    crisp::CommandLineParser clp;
    clp.addOption("width", width);
    clp.addOption("path", path);
    clp.addOption("height", height);
    clp.addOption("find_all_enemies", findAllEnemies);

    clp.parse(argLine).unwrap();

    ASSERT_EQ(width, 443);
    ASSERT_EQ(height, 23);
    ASSERT_EQ(path, "some_directive");
    ASSERT_EQ(findAllEnemies, "not");
}
