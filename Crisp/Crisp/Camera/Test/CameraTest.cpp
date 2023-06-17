#include <gtest/gtest.h>

#include <Crisp/Camera/Camera.hpp>

namespace crisp::test
{
constexpr uint32_t kDefaultWidth = 512;
constexpr uint32_t kDefaultHeight = 512;

constexpr double kHalfPi = glm::pi<double>() * 0.5;

TEST(CameraTest, Basic)
{
    Camera cam(kDefaultWidth, kDefaultHeight);
    EXPECT_EQ(cam.getViewDepthRange(), glm::vec2(0.1f, 1000.0f));

    EXPECT_EQ(cam.getLookDir(), glm::vec3(0.0f, 0.0f, -1.0f));
    EXPECT_EQ(cam.getRightDir(), glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(cam.getUpDir(), glm::vec3(0.0f, 1.0f, 0.0f));

    cam.setOrientation(glm::angleAxis(kHalfPi, glm::dvec3(0.0, 1.0, 0.0)));
    EXPECT_NEAR(glm::l2Norm(cam.getLookDir() - glm::vec3(-1.0f, 0.0f, 0.0f)), 0.0f, 1e-6);
    EXPECT_NEAR(glm::l2Norm(cam.getRightDir() - glm::vec3(0.0f, 0.0f, -1.0f)), 0.0f, 1e-6);
    EXPECT_NEAR(glm::l2Norm(cam.getUpDir() - glm::vec3(0.0f, 1.0f, 0.0f)), 0.0f, 1e-6);
}

TEST(CameraTest, DepthRange)
{
    constexpr float kZNear = 1.0f;
    constexpr float kZFar = 10000.0f;
    EXPECT_EQ(Camera(kDefaultWidth, kDefaultHeight, kZNear, kZFar).getViewDepthRange(), glm::vec2(kZNear, kZFar));
}

} // namespace crisp::test