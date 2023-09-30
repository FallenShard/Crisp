
#include <Crisp/Camera/FreeCameraController.hpp>

#include <gmock/gmock.h>

namespace crisp::test {
namespace {
constexpr double kEpsilon = 1e-6;

MATCHER_P2(GlmVecNearEq, other, eps, "Matches two glm vectors for approximate equality") {
    for (int32_t i = 0; i < decltype(other)::length(); ++i) {
        if (std::abs(arg[i] - other[i]) >= eps) {
            *result_listener << "Left: " << arg[i] << " Right: " << other[i] << '\n';
            return false;
        }
    }
    return true;
}

MATCHER_P(GlmMatNearEq, p, "Matches two glm matrices for approximate equality.") {
    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = 0; j < 4; ++j) {
            if (std::abs(arg[i][j] - p[i][j]) >= kEpsilon) {
                *result_listener << "Left: " << arg[i][j] << " Right: " << p[i][j] << '\n';
                return false;
            }
        }
    }
    return true;
}

} // namespace

TEST(FreeCameraControllerTest, DefaultState) {
    FreeCameraController controller(512, 512);

    const auto viewMat = controller.getCamera().getViewMatrix();
    const auto lookAt = glm::lookAt(glm::vec3(0, 1, 10), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
    EXPECT_THAT(viewMat, GlmMatNearEq(lookAt));
    EXPECT_THAT(controller.getCamera().getRightDir(), GlmVecNearEq(glm::vec3(1.0f, 0.0f, 0.0f), kEpsilon));
    EXPECT_THAT(controller.getCamera().getUpDir(), GlmVecNearEq(glm::vec3(0.0f, 1.0f, 0.0f), kEpsilon));
    EXPECT_THAT(controller.getCamera().getLookDir(), GlmVecNearEq(glm::vec3(0.0f, 0.0f, -1.0f), kEpsilon));
}

TEST(FreeCameraControllerTest, ComplexMotion) {
    FreeCameraController controller(512, 512);
    constexpr float kSpeed = 1.5f;
    controller.setSpeed(kSpeed);

    // Strafe right 3 units.
    controller.move(3.0f, 0.0f);
    const auto lookAt = glm::lookAt(glm::vec3(4.5, 1, 10), glm::vec3(4.5, 1, 0), glm::vec3(0, 1, 0));
    EXPECT_THAT(controller.getCamera().getViewMatrix(), GlmMatNearEq(lookAt));

    // Rotate around Y axis for 90 degrees to the left.
    controller.updateOrientation(1.0f, 0.0f);
    const auto lookAt2 = glm::lookAt(glm::vec3(4.5, 1, 10), glm::vec3(0, 1, 10), glm::vec3(0, 1, 0));
    EXPECT_THAT(controller.getCamera().getViewMatrix(), GlmMatNearEq(lookAt2));
    EXPECT_THAT(controller.getCamera().getRightDir(), GlmVecNearEq(glm::vec3(0.0f, 0.0f, -1.0f), kEpsilon));
    EXPECT_THAT(controller.getCamera().getLookDir(), GlmVecNearEq(glm::vec3(-1.0f, 0.0f, 0.0f), kEpsilon));
    EXPECT_THAT(controller.getCamera().getUpDir(), GlmVecNearEq(glm::vec3(0.0f, 1.0f, 0.0f), kEpsilon));

    // Strafe right 4 units and move forward 2 units.
    controller.move(4.0f, 2.0f);
    const auto lookAt3 = glm::lookAt(glm::vec3(1.5, 1, 4), glm::vec3(0, 1, 4), glm::vec3(0, 1, 0));
    EXPECT_THAT(controller.getCamera().getViewMatrix(), GlmMatNearEq(lookAt3));
}
} // namespace crisp::test