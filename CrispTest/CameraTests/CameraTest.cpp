#include <gtest/gtest.h>


#include <Crisp/Camera/Camera.hpp>


TEST(CameraTest, Basic)
{
    crisp::Camera cam(512, 512);
    EXPECT_EQ(cam.getViewDepthRange(), glm::vec2(0.1f, 1000.0f));

    EXPECT_EQ(cam.getLookDir(), glm::vec3(0.0f, 0.0f, -1.0f));
    EXPECT_EQ(cam.getRightDir(), glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(cam.getUpDir(), glm::vec3(0.0f, 1.0f, 0.0f));

    cam.setOrientation(glm::angleAxis(glm::radians(90.0), glm::dvec3(0.0, 1.0, 0.0)));
    EXPECT_NEAR(glm::l2Norm(cam.getLookDir() - glm::vec3(-1.0f, 0.0f, 0.0f)), 0.0f, 1e-6);
    EXPECT_NEAR(glm::l2Norm(cam.getRightDir() - glm::vec3(0.0f, 0.0f, -1.0f)), 0.0f, 1e-6);
    EXPECT_NEAR(glm::l2Norm(cam.getUpDir() - glm::vec3(0.0f, 1.0f, 0.0f)), 0.0f, 1e-6);
}
