#include <Crisp/Math/Distribution1D.hpp>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(Distribution1DTest, StartsEmptyAndClearResetsAllState) {
    Distribution1D distribution(4);

    EXPECT_EQ(distribution.getSize(), 0);
    EXPECT_FALSE(distribution.isNormalized());
    EXPECT_FLOAT_EQ(distribution.getSum(), 0.0f);
    EXPECT_FLOAT_EQ(distribution.getNormFactor(), 0.0f);

    distribution.append(2.0f);
    distribution.normalize();
    distribution.clear();

    EXPECT_EQ(distribution.getSize(), 0);
    EXPECT_FALSE(distribution.isNormalized());
    EXPECT_FLOAT_EQ(distribution.getSum(), 0.0f);
    EXPECT_FLOAT_EQ(distribution.getNormFactor(), 0.0f);
}

TEST(Distribution1DTest, NormalizesAndPreservesOriginalSum) {
    Distribution1D distribution;
    distribution.append(1.0f);
    distribution.append(2.0f);
    distribution.append(3.0f);

    EXPECT_FLOAT_EQ(distribution.normalize(), 6.0f);
    EXPECT_TRUE(distribution.isNormalized());
    EXPECT_FLOAT_EQ(distribution.getSum(), 6.0f);
    EXPECT_FLOAT_EQ(distribution.getNormFactor(), 1.0f / 6.0f);
    EXPECT_FLOAT_EQ(distribution[0], 1.0f / 6.0f);
    EXPECT_FLOAT_EQ(distribution[1], 2.0f / 6.0f);
    EXPECT_FLOAT_EQ(distribution[2], 3.0f / 6.0f);
}

TEST(Distribution1DTest, SkipsZeroProbabilityIntervalsAtCdfBoundaries) {
    Distribution1D distribution;
    distribution.append(0.0f);
    distribution.append(1.0f);
    distribution.append(0.0f);
    distribution.append(3.0f);
    distribution.normalize();

    float sample = 0.0f;
    float probability = 0.0f;
    EXPECT_EQ(distribution.sampleReuse(sample, probability), 1);
    EXPECT_FLOAT_EQ(sample, 0.0f);
    EXPECT_FLOAT_EQ(probability, 0.25f);

    sample = 0.25f;
    EXPECT_EQ(distribution.sampleReuse(sample, probability), 3);
    EXPECT_FLOAT_EQ(sample, 0.0f);
    EXPECT_FLOAT_EQ(probability, 0.75f);
}

TEST(Distribution1DTest, UsesUniformFallbackForAllZeroWeights) {
    Distribution1D distribution;
    for (int i = 0; i < 4; ++i) {
        distribution.append(0.0f);
    }

    EXPECT_FLOAT_EQ(distribution.normalize(), 0.0f);
    EXPECT_FLOAT_EQ(distribution.getSum(), 0.0f);
    EXPECT_FLOAT_EQ(distribution.getNormFactor(), 0.0f);
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(distribution[i], 0.25f);
    }

    float sample = 0.625f;
    float probability = 0.0f;
    EXPECT_EQ(distribution.sampleReuse(sample, probability), 2);
    EXPECT_FLOAT_EQ(sample, 0.5f);
    EXPECT_FLOAT_EQ(probability, 0.25f);
}

TEST(Distribution1DTest, DistinguishesCellProbabilityFromContinuousDensity) {
    Distribution1D distribution;
    distribution.append(1.0f);
    distribution.append(3.0f);
    distribution.normalize();

    float probability = 0.0f;
    EXPECT_EQ(distribution.sample(0.125f, probability), 0);
    EXPECT_FLOAT_EQ(probability, 0.25f);

    float pdf = 0.0f;
    size_t offset = 0;
    EXPECT_FLOAT_EQ(distribution.sampleContinuous(0.125f, pdf, offset), 0.25f);
    EXPECT_EQ(offset, 0);
    EXPECT_FLOAT_EQ(pdf, 0.5f);

    EXPECT_FLOAT_EQ(distribution.sampleContinuous(0.25f, pdf, offset), 0.5f);
    EXPECT_EQ(offset, 1);
    EXPECT_FLOAT_EQ(pdf, 1.5f);
}

} // namespace
} // namespace crisp
