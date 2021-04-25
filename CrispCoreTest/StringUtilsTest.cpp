#include "gtest/gtest.h"

#include "CrispCore/StringUtils.hpp"

using namespace crisp;

#include <CrispCore/Math/Warp.hpp>
#include <CrispCore/Math/CoordinateFrame.hpp>

#include <random>


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

namespace
{
    using uint = unsigned int;
    uint tea(uint val0, uint val1)
    {
        uint v0 = val0;
        uint v1 = val1;
        uint s0 = 0;

        for (uint n = 0; n < 16; n++)
        {
            s0 += 0x9e3779b9;
            v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
            v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
        }

        return v0;
    }

    // Generate a random unsigned int in [0, 2^24) given the previous RNG state
    // using the Numerical Recipes linear congruential generator
    uint lcg(uint& prev)
    {
        uint LCG_A = 1664525u;
        uint LCG_C = 1013904223u;
        prev = (LCG_A * prev + LCG_C);
        return prev & 0x00FFFFFF;
    }

    uint RandomInt(uint& seed)
    {
        // LCG values from Numerical Recipes
        return (seed = 1664525 * seed + 1013904223);
    }

    float RandomFloat(uint& seed)
    {
        // Float version using bitmask from Numerical Recipes
        const uint one = 0x3f800000;
        const uint msk = 0x007fffff;
        const uint v = one | (msk & (RandomInt(seed) >> 9));
        float ret;
        std::memcpy(&ret, &v, sizeof(float));
        return ret - 1;
    }
}

//TEST(TemporaryTest, Warping)
//{
//    glm::vec3 normal(0.0f, 0.0f, 1.0f);
//    CoordinateFrame frame(normal);
//
//    uint seed = tea(0, 0);
//
//    std::default_random_engine eng;
//    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
//    for (int i = 0; i < 1000; ++i)
//    {
//        //glm::vec2 sample(dist(eng), dist(eng));
//        glm::vec2 sample(RandomFloat(seed), RandomFloat(seed));
//        glm::vec3 dir = crisp::warp::squareToUniformHemisphere(sample);
//        dir = frame.toWorld(dir);
//        EXPECT_TRUE(dir.z >= 0.0f);
//    }
//}
