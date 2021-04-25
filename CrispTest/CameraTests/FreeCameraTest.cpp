#include "gtest/gtest.h"

#include "Camera/FreeCamera.hpp"

using namespace crisp;

namespace
{
}

TEST(FreeCameraTest, Basic)
{
    FreeCamera cam;
    ASSERT_EQ(cam.getAspectRatio(), 1.0f);
    ASSERT_EQ(cam.getNearPlaneDistance(), 1.0f);

    ASSERT_EQ(cam.getLookDirection(), glm::vec3(0.0f, 0.0f, -1.0f));
    ASSERT_EQ(cam.getRightDirection(), glm::vec3(1.0f, 0.0f, 0.0f));
    ASSERT_EQ(cam.getUpDirection(), glm::vec3(0.0f, 1.0f, 0.0f));

    cam.setRotation(glm::pi<float>() * 0.5f, 0.0f, 0.0f);
    cam.update(0.0f);
    ASSERT_NEAR(glm::l2Norm(cam.getLookDirection() - glm::vec3(-1.0f, 0.0f, 0.0f)), 0.0f, 1e-6);
    ASSERT_NEAR(glm::l2Norm(cam.getRightDirection() - glm::vec3(0.0f, 0.0f, -1.0f)), 0.0f, 1e-6);
    ASSERT_NEAR(glm::l2Norm(cam.getUpDirection() - glm::vec3(0.0f, 1.0f, 0.0f)), 0.0f, 1e-6);
}
