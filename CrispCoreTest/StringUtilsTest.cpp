#include "gtest/gtest.h"

#include "CrispCore/StringUtils.hpp"

using namespace crisp;


TEST(StringUtilsTest, Tokenize)
{
    std::string text = "Hello World";

    auto tokens = tokenize(text, " ");

    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "Hello");
    EXPECT_EQ(tokens[1], "World");
}
