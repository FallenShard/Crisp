#include "gtest/gtest.h"

#include <CrispCore/StringUtils.hpp>

using namespace crisp;


TEST(StringUtilsTest, Tokenize)
{
    std::string text = "Hello World";

    auto tokens = tokenize(text, " ");

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "Hello");
    EXPECT_EQ(tokens[1], "World");
}

TEST(StringUtilsTest, FixedTokenize)
{
    std::string text = "10/15/20";
    auto tokens = fixedTokenize<3>(text, "/");

    ASSERT_EQ(tokens[0], "10");
    ASSERT_EQ(tokens[1], "15");
    ASSERT_EQ(tokens[2], "20");


    text = "10//20";
    tokens = fixedTokenize<3>(text, "/", false);
    ASSERT_EQ(tokens[0], "10");
    ASSERT_EQ(tokens[1], "");
    ASSERT_EQ(tokens[2], "20");

    text = "//20/";
    tokens = fixedTokenize<3>(text, "/", false);
    ASSERT_EQ(tokens[0], "");
    ASSERT_EQ(tokens[1], "");
    ASSERT_EQ(tokens[2], "20");

    text = "1/";
    tokens = fixedTokenize<3>(text, "/");
    ASSERT_EQ(tokens[0], "1");
    ASSERT_EQ(tokens[1], "");
    ASSERT_EQ(tokens[2], "");
}
