#include <gtest/gtest.h>

#include <array>

#include <Crisp/Lights/CascadedShadowMapping.hpp>

namespace crisp {
namespace {

TEST(CascadedShadowMappingTest, ComputesAbsolutePracticalSplitDepths) {
    CascadedShadowMapping csm;
    csm.cascades.resize(4);
    csm.splitLambda = 0.5f;

    csm.updateSplitIntervals(1.0f, 100.0f);

    constexpr std::array<float, 5> kExpectedSplitDepths{1.0f, 14.456139f, 30.25f, 53.43639f, 100.0f};
    for (size_t i = 0; i < csm.cascades.size(); ++i) {
        EXPECT_NEAR(csm.cascades[i].zNear, kExpectedSplitDepths[i], 1e-5f);
        EXPECT_NEAR(csm.cascades[i].zFar, kExpectedSplitDepths[i + 1], 1e-5f);
    }
}

TEST(CascadedShadowMappingTest, ProducesContiguousLinearSplits) {
    CascadedShadowMapping csm;
    csm.cascades.resize(4);
    csm.splitLambda = 0.0f;

    csm.updateSplitIntervals(2.0f, 10.0f);

    constexpr std::array<float, 5> kExpectedSplitDepths{2.0f, 4.0f, 6.0f, 8.0f, 10.0f};
    for (size_t i = 0; i < csm.cascades.size(); ++i) {
        EXPECT_FLOAT_EQ(csm.cascades[i].zNear, kExpectedSplitDepths[i]);
        EXPECT_FLOAT_EQ(csm.cascades[i].zFar, kExpectedSplitDepths[i + 1]);
    }
}

TEST(CascadedShadowMappingTest, ComputesOptionalBlendIntervals) {
    CascadedShadowMapping csm;
    csm.cascades.resize(2);
    csm.splitLambda = 0.0f;
    csm.splitBlendFraction = 0.25f;

    csm.updateSplitIntervals(2.0f, 10.0f);

    EXPECT_FLOAT_EQ(csm.cascades[0].zNear, 2.0f);
    EXPECT_FLOAT_EQ(csm.cascades[0].zFar, 6.0f);
    EXPECT_FLOAT_EQ(csm.cascades[0].blendStart, 5.0f);
    EXPECT_FLOAT_EQ(csm.cascades[1].blendStart, 10.0f);
}

TEST(CascadedShadowMappingTest, ZeroBlendFractionDisablesOverlap) {
    CascadedShadowMapping csm;
    csm.cascades.resize(2);
    csm.splitLambda = 0.0f;
    csm.splitBlendFraction = 0.0f;

    csm.updateSplitIntervals(2.0f, 10.0f);

    EXPECT_FLOAT_EQ(csm.cascades[0].blendStart, csm.cascades[0].zFar);
}

TEST(CascadedShadowMappingTest, CullsAgainstExtrudedCasterVolume) {
    CascadedShadowMapping csm;
    csm.cascades.resize(1);
    csm.cascades[0].light =
        DirectionalLight(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f), glm::vec3(-1.0f), glm::vec3(1.0f));
    csm.cascades[0].light.fitProjectionToBoundingSphere(glm::vec3(0.0f), 10.0f, 1024, 20.0f);

    EXPECT_TRUE(csm.isCasterVisible(0, BoundingBox3(glm::vec3(-1.0f, -1.0f, 14.0f), glm::vec3(1.0f, 1.0f, 16.0f))));
    EXPECT_FALSE(csm.isCasterVisible(0, BoundingBox3(glm::vec3(20.0f, -1.0f, 14.0f), glm::vec3(22.0f, 1.0f, 16.0f))));
    EXPECT_FALSE(csm.isCasterVisible(0, BoundingBox3(glm::vec3(-1.0f, -1.0f, -22.0f), glm::vec3(1.0f, 1.0f, -20.0f))));
}

} // namespace
} // namespace crisp
