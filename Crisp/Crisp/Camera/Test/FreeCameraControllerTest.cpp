
#include <Crisp/Camera/FreeCameraController.hpp>

#include <gmock/gmock.h>

namespace crisp::test
{
namespace
{
constexpr double kEpsilon = 1e-6;

MATCHER_P(GlmMatNearEq, p, "Matches two glm matrices for approximate equality.")
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        for (uint32_t j = 0; j < 4; ++j)
        {
            if (std::abs(arg[i][j] - p[i][j]) >= kEpsilon)
            {
                *result_listener << "Left: " << arg[i][j] << " Right: " << p[i][j] << '\n';
                return false;
            }
        }
    }
    return true;
}

inline void testMatEq(const glm::mat4& a, const glm::mat4& b)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        for (uint32_t j = 0; j < 4; ++j)
        {
            ASSERT_NEAR(a[i][j], b[i][j], 1e-6);
        }
    }
}

// inline void printMat(const glm::mat4& a)
// {
//     static constexpr uint32_t kWidth = 6;
//     for (int32_t i = 0; i < 4; ++i)
//     {
//         for (int32_t j = 0; j < 4; ++j)
//         {
//             std::cout << std::setprecision(2) << std::setw(kWidth) << a[j][i] << " ";
//         }
//         std::cout << '\n';
//     }
// }

template <typename ScalarType, glm::length_t Dims>
inline void testVecEq(const glm::vec<Dims, ScalarType>& a, const glm::vec<Dims, ScalarType>& b)
{
    for (uint32_t i = 0; i < Dims; ++i)
    {
        ASSERT_NEAR(a[i], b[i], 1e-6);
    }
}
} // namespace

TEST(FreeCameraControllerTest, DefaultState)
{
    FreeCameraController controller(512, 512);

    const auto viewMat = controller.getCamera().getViewMatrix();
    const auto lookAt = glm::lookAt(glm::vec3(0, 1, 10), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
    testMatEq(viewMat, lookAt);
    testVecEq(controller.getCamera().getRightDir(), glm::vec3(1.0f, 0.0f, 0.0f));
    testVecEq(controller.getCamera().getUpDir(), glm::vec3(0.0f, 1.0f, 0.0f));
    testVecEq(controller.getCamera().getLookDir(), glm::vec3(0.0f, 0.0f, -1.0f));
}

TEST(FreeCameraControllerTest, ComplexMotion)
{
    FreeCameraController controller(512, 512);
    constexpr float kSpeed = 1.5f;
    controller.setSpeed(kSpeed);

    // Strafe right 3 units.
    controller.move(3.0f, 0.0f);
    const auto lookAt = glm::lookAt(glm::vec3(4.5, 1, 10), glm::vec3(4.5, 1, 0), glm::vec3(0, 1, 0));
    testMatEq(controller.getCamera().getViewMatrix(), lookAt);

    // Rotate around Y axis for 90 degrees to the left.
    controller.updateOrientation(1.0f, 0.0f);
    const auto lookAt2 = glm::lookAt(glm::vec3(4.5, 1, 10), glm::vec3(0, 1, 10), glm::vec3(0, 1, 0));
    testMatEq(controller.getCamera().getViewMatrix(), lookAt2);
    testVecEq(controller.getCamera().getRightDir(), glm::vec3(0.0f, 0.0f, -1.0f));
    testVecEq(controller.getCamera().getLookDir(), glm::vec3(-1.0f, 0.0f, 0.0f));
    testVecEq(controller.getCamera().getUpDir(), glm::vec3(0.0f, 1.0f, 0.0f));

    // Strafe right 4 units and move forward 2 units.
    controller.move(4.0f, 2.0f);
    const auto lookAt3 = glm::lookAt(glm::vec3(1.5, 1, 4), glm::vec3(0, 1, 4), glm::vec3(0, 1, 0));
    testMatEq(controller.getCamera().getViewMatrix(), lookAt3);
}
} // namespace crisp::test