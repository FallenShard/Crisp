#include <Crisp/Core/CommandLineParser.hpp>

#include <optional>

#include <gtest/gtest.h>

TEST(CommandLineParserTest, Basic) {
    const char* argLine = "Program123 width 443 --path some_directive height=23";

    int32_t width = 0;
    int16_t height = 0;
    std::string path;
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

TEST(CommandLineParserTest, ParsesOptionalValuesAndExplicitBooleans) {
    std::optional<std::string> scene;
    std::optional<bool> rayTracing;

    crisp::CommandLineParser parser;
    parser.addOption("scene", scene);
    parser.addOption("ray_tracing", rayTracing);

    ASSERT_TRUE(parser.parse("CrispMain --scene=atmosphere --ray_tracing off").isValid());
    EXPECT_EQ(scene, "atmosphere");
    EXPECT_EQ(rayTracing, false);
}

TEST(CommandLineParserTest, RejectsUnknownOptions) {
    crisp::CommandLineParser parser;
    std::string scene;
    parser.addOption("scene", scene);

    EXPECT_FALSE(parser.parse("CrispMain --scnee atmosphere").isValid());
}

TEST(CommandLineParserTest, RejectsMissingAndMalformedValues) {
    crisp::CommandLineParser parser;
    bool enabled = false;
    parser.addOption("enabled", enabled);

    EXPECT_FALSE(parser.parse("CrispMain --enabled").isValid());
    EXPECT_FALSE(parser.parse("CrispMain --enabled perhaps").isValid());
}

TEST(CommandLineParserTest, RejectsMalformedNumericValuesWithoutChangingDestination) {
    crisp::CommandLineParser parser;
    int32_t value = 7;
    parser.addOption("value", value);

    EXPECT_FALSE(parser.parse("CrispMain --value 12garbage").isValid());
    EXPECT_EQ(value, 7);
    EXPECT_FALSE(parser.parse("CrispMain --value 999999999999999999999999").isValid());
    EXPECT_EQ(value, 7);
}

TEST(CommandLineParserTest, DoesNotConsumeFollowingOptionAsAValue) {
    crisp::CommandLineParser parser;
    std::optional<std::string> scene;
    std::optional<std::string> logLevel;
    parser.addOption("scene", scene);
    parser.addOption("log_level", logLevel);

    EXPECT_FALSE(parser.parse("CrispMain --scene --log_level debug").isValid());
    EXPECT_FALSE(scene.has_value());
    EXPECT_FALSE(logLevel.has_value());
}

TEST(CommandLineParserTest, DuplicateRegistrationUsesLatestBinding) {
    crisp::CommandLineParser parser;
    std::string first;
    std::string second;
    parser.addOption("scene", first);
    parser.addOption("scene", second);

    ASSERT_TRUE(parser.parse("CrispMain --scene atmosphere").isValid());
    EXPECT_TRUE(first.empty());
    EXPECT_EQ(second, "atmosphere");
}
