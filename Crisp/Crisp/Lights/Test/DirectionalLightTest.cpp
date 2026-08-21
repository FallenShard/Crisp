#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

#include <Crisp/Lights/DirectionalLight.hpp>

namespace crisp {
namespace {

bool isFinite(const glm::mat4& matrix) {
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}

void expectMatricesNear(const glm::mat4& lhs, const glm::mat4& rhs, const float tolerance) {
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            EXPECT_NEAR(lhs[column][row], rhs[column][row], tolerance);
        }
    }
}

glm::vec3 getViewRight(const DirectionalLight& light) {
    return glm::normalize(glm::vec3(glm::inverse(light.getViewMatrix())[0]));
}

DirectionalLight createLight(const glm::vec3& direction) {
    return {direction, glm::vec3(1.0f), glm::vec3(-5.0f), glm::vec3(5.0f)};
}

TEST(DirectionalLightTest, SupportsDirectionsParallelToWorldUp) {
    constexpr std::array kVerticalDirections{glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)};

    for (const auto& direction : kVerticalDirections) {
        auto light = createLight(direction);
        light.fitProjectionToBoundingSphere(glm::vec3(0.0f), 10.0f, 1024);

        const auto descriptor = light.createDescriptor();
        EXPECT_TRUE(isFinite(descriptor.V));
        EXPECT_TRUE(isFinite(descriptor.P));
        EXPECT_TRUE(isFinite(descriptor.VP));
        EXPECT_NEAR(glm::length(light.getDirection()), 1.0f, 1e-6f);
    }
}

TEST(DirectionalLightTest, PreservesBasisContinuityAcrossVerticalDirection) {
    auto light = createLight(glm::vec3(0.05f, -1.0f, 0.0f));
    const glm::vec3 rightBefore = getViewRight(light);

    light.setDirection(glm::vec3(0.0f, -1.0f, 0.0f));
    const glm::vec3 rightAtVertical = getViewRight(light);
    light.setDirection(glm::vec3(-0.05f, -1.0f, 0.0f));
    const glm::vec3 rightAfter = getViewRight(light);

    EXPECT_GT(glm::dot(rightBefore, rightAtVertical), 0.99f);
    EXPECT_GT(glm::dot(rightAtVertical, rightAfter), 0.99f);
}

TEST(DirectionalLightTest, KeepsBoundingSphereInsideSnappedProjection) {
    constexpr float kRadius{10.0f};
    constexpr uint32_t kShadowMapSize{1024};
    const float xyExtent = kRadius / (1.0f - 2.0f / static_cast<float>(kShadowMapSize));
    const float worldUnitsPerTexel = 2.0f * xyExtent / static_cast<float>(kShadowMapSize);
    const glm::vec3 center(0.51f * worldUnitsPerTexel, -0.51f * worldUnitsPerTexel, 3.0f);

    auto light = createLight(glm::vec3(0.0f, 0.0f, -1.0f));
    light.fitProjectionToBoundingSphere(center, kRadius, kShadowMapSize);
    const glm::mat4 lightVp = light.createDescriptor().VP;

    constexpr std::array kSphereExtrema{
        glm::vec3(-kRadius, 0.0f, 0.0f),
        glm::vec3(+kRadius, 0.0f, 0.0f),
        glm::vec3(0.0f, -kRadius, 0.0f),
        glm::vec3(0.0f, +kRadius, 0.0f),
    };
    for (const glm::vec3& offset : kSphereExtrema) {
        const glm::vec4 clipPosition = lightVp * glm::vec4(center + offset, 1.0f);
        const glm::vec3 ndcPosition = glm::vec3(clipPosition) / clipPosition.w;
        EXPECT_LE(std::abs(ndcPosition.x), 1.0f);
        EXPECT_LE(std::abs(ndcPosition.y), 1.0f);
    }
}

TEST(DirectionalLightTest, IgnoresSubTexelReceiverMotion) {
    constexpr float kRadius{10.0f};
    constexpr uint32_t kShadowMapSize{1024};
    const float xyExtent = kRadius / (1.0f - 2.0f / static_cast<float>(kShadowMapSize));
    const float worldUnitsPerTexel = 2.0f * xyExtent / static_cast<float>(kShadowMapSize);

    auto light = createLight(glm::vec3(0.0f, 0.0f, -1.0f));
    light.fitProjectionToBoundingSphere(glm::vec3(0.0f, 0.0f, 3.0f), kRadius, kShadowMapSize);
    const glm::mat4 initialVp = light.createDescriptor().VP;

    light.fitProjectionToBoundingSphere(
        glm::vec3(0.49f * worldUnitsPerTexel, 0.49f * worldUnitsPerTexel, 3.0f), kRadius, kShadowMapSize);
    expectMatricesNear(light.createDescriptor().VP, initialVp, 1e-6f);
}

TEST(DirectionalLightTest, NormalizesValidDirections) {
    const auto light = createLight(glm::vec3(2.0f, -3.0f, 4.0f));
    EXPECT_NEAR(glm::length(light.getDirection()), 1.0f, 1e-6f);
    EXPECT_TRUE(isFinite(light.getViewMatrix()));
}

TEST(DirectionalLightTest, RejectsZeroDirection) {
    EXPECT_DEATH_IF_SUPPORTED({ static_cast<void>(createLight(glm::vec3(0.0f))); }, "");
}

TEST(DirectionalLightTest, RejectsNonFiniteDirection) {
    EXPECT_DEATH_IF_SUPPORTED(
        { static_cast<void>(createLight(glm::vec3(std::numeric_limits<float>::infinity(), 0.0f, 0.0f))); }, "");
}

} // namespace
} // namespace crisp
