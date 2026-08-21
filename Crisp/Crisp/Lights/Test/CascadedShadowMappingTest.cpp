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

} // namespace
} // namespace crisp
