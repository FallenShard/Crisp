#include "Warp.hpp"

#include <algorithm>

#include "Constants.hpp"
#include "Headers.hpp"

namespace crisp
{
    namespace
    {
        glm::vec3 squareToUniformCylinder(const glm::vec2& sample, float cosThetaMin, float cosThetaMax)
        {
            float z = cosThetaMin + sample.y * (cosThetaMax - cosThetaMin);
            float phi = 2.0f * PI * sample.x;

            return glm::vec3(cosf(phi), sinf(phi), z);
        }

        glm::vec3 cylinderToSphereSection(const glm::vec2& sample, float cosThetaMin, float cosThetaMax)
        {
            glm::vec3 cylinderPt = squareToUniformCylinder(sample, cosThetaMin, cosThetaMax);
            float radius = sqrtf(1.0f - cylinderPt.z * cylinderPt.z);
            return glm::vec3(cylinderPt.x * radius, cylinderPt.y * radius, cylinderPt.z);
        }
    }

    namespace warp
    {
        glm::vec2 squareToUniformSquare(const glm::vec2& sample)
        {
            return sample;
        }

        float squareToUniformSquarePdf()
        {
            return 1.0f;
        }

        glm::vec2 squareToUniformDisk(const glm::vec2& sample)
        {
            float r = sqrtf(sample.y);
            float theta = 2.0f * PI * sample.x;
            return { r * cosf(theta), r * sinf(theta) };
        }

        float squareToUniformDiskPdf()
        {
            return InvPI;
        }

        glm::vec3 squareToUniformSphere(const glm::vec2& sample)
        {
            return cylinderToSphereSection(sample, 1.0f, -1.0f);
        }

        float squareToUniformSpherePdf()
        {
            return InvFourPI;
        }

        glm::vec3 squareToUniformHemisphere(const glm::vec2& sample)
        {
            return cylinderToSphereSection(sample, 1.0f, 0.0f);
        }

        float squareToUniformHemispherePdf(const glm::vec3&)
        {
            return InvTwoPI;
        }

        glm::vec3 squareToUniformSphereCap(const glm::vec2& sample, float cosThetaMax)
        {
            return cylinderToSphereSection(sample, 1.0f, cosThetaMax);
        }

        float squareToUniformSphereCapPdf(float cosThetaMax)
        {
            return InvTwoPI / (1.0f - cosThetaMax);
        }

        glm::vec3 squareToCosineHemisphere(const glm::vec2& sample)
        {
            float radius = std::sqrt(sample.y);
            float theta = 2.0f * PI * sample.x;
            float x = radius * std::cos(theta);
            float y = radius * std::sin(theta);
            float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));
            return { x, y, z };
        }

        float squareToCosineHemispherePdf(const glm::vec3& v)
        {
            return v.z * InvPI;
        }

        glm::vec3 squareToUniformTriangle(const glm::vec2& sample)
        {
            float val = sqrtf(sample.x);
            float u = 1.0f - val;
            float v = sample.y * val;
            return glm::vec3(u, v, 1.f - u - v);
        }
    }
}