#include <Crisp/Math/GlmFormat.hpp>

#include <gtest/gtest.h>

TEST(GlmStructuredBindingsTest, RValue) {
    const auto createVec = []() { return glm::vec3(10.0f, 20.0f, 30.0f); };

    auto [x, y, z] = createVec();

    EXPECT_EQ(x, 10.0f);
    EXPECT_EQ(y, 20.0f);
    EXPECT_EQ(z, 30.0f);
}

TEST(GlmStructuredBindingsTest, LValue) {
    glm::vec3 vec{-1.0f, 0.0f, 1.0f};
    auto [x, y, z] = vec;

    EXPECT_EQ(x, vec[0]);
    EXPECT_EQ(y, vec[1]);
    EXPECT_EQ(z, vec[2]);

    EXPECT_EQ(-1.0f, vec[0]);
    EXPECT_EQ(0.0f, vec[1]);
    EXPECT_EQ(+1.0f, vec[2]);

    auto& [x2, y2, z2] = vec;

    x2 = 10.0f;
    EXPECT_EQ(10.0f, vec[0]);
}

TEST(GlmStructuredBindingsTest, CLValue) {
    const glm::vec3 vec{-1.0f, 0.0f, 1.0f};
    const auto [x, y, z] = vec;

    EXPECT_EQ(x, vec[0]);
    EXPECT_EQ(y, vec[1]);
    EXPECT_EQ(z, vec[2]);
}

TEST(GlmStructuredBindingsTest, Printing) {
    const glm::vec3 vec(0.0f, 1.0f, 2.0f);

    const auto format = fmt::format("{}", vec);
}