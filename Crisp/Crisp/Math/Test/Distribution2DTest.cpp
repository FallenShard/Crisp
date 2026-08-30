#include <Crisp/Math/Distribution2D.hpp>

#include <array>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(Distribution2DTest, NormalizesCellProbabilitiesAndUnitSquareDensity) {
    constexpr std::array weights{1.0f, 3.0f, 2.0f, 4.0f};
    const Distribution2D distribution(weights, 2, 2);

    EXPECT_EQ(distribution.getColumnCount(), 2);
    EXPECT_EQ(distribution.getRowCount(), 2);
    EXPECT_DOUBLE_EQ(distribution.getWeightSum(), 10.0);
    EXPECT_FLOAT_EQ(distribution.getCellProbability(0, 0), 0.1f);
    EXPECT_FLOAT_EQ(distribution.getCellProbability(1, 0), 0.3f);
    EXPECT_FLOAT_EQ(distribution.getCellProbability(0, 1), 0.2f);
    EXPECT_FLOAT_EQ(distribution.getCellProbability(1, 1), 0.4f);
    EXPECT_FLOAT_EQ(distribution.getPdf({0.25f, 0.25f}), 0.4f);
    EXPECT_FLOAT_EQ(distribution.getPdf({0.75f, 0.25f}), 1.2f);
    EXPECT_EQ(distribution.getCdf().size(), 9);
}

TEST(Distribution2DTest, SamplesMarginalThenConditionalAndRemapsResiduals) {
    constexpr std::array weights{1.0f, 3.0f, 2.0f, 4.0f};
    const Distribution2D distribution(weights, 2, 2);
    const auto sample = distribution.sampleContinuous({0.5f, 0.1f});

    EXPECT_EQ(sample.cell, glm::uvec2(1, 0));
    EXPECT_NEAR(sample.value.x, 2.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(sample.value.y, 0.125f, 1e-6f);
    EXPECT_FLOAT_EQ(sample.pdf, 1.2f);
    EXPECT_FLOAT_EQ(sample.pdf, distribution.getPdf(sample.value));
}

TEST(Distribution2DTest, UsesUniformFallbackForAZeroWeightGrid) {
    constexpr std::array weights{0.0f, 0.0f, 0.0f, 0.0f};
    const Distribution2D distribution(weights, 2, 2);

    EXPECT_DOUBLE_EQ(distribution.getWeightSum(), 0.0);
    for (uint32_t row = 0; row < 2; ++row) {
        for (uint32_t column = 0; column < 2; ++column) {
            EXPECT_FLOAT_EQ(distribution.getCellProbability(column, row), 0.25f);
        }
    }
    EXPECT_FLOAT_EQ(distribution.getPdf({0.3f, 0.7f}), 1.0f);
}

} // namespace
} // namespace crisp
