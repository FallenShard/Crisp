#pragma once

#include <glm/glm.hpp>

namespace vesper
{
    class Warp
    {
    public:
        static glm::vec2 squareToUniformSquare(const glm::vec2& sample);
        static float squareToUniformSquarePdf();

        static glm::vec2 squareToUniformDisk(const glm::vec2& sample);
        static float squareToUniformDiskPdf();

        static glm::vec3 squareToUniformSphere(const glm::vec2& sample);
        static float squareToUniformSpherePdf();

        static glm::vec3 squareToUniformHemisphere(const glm::vec2& sample);
        static float squareToUniformHemispherePdf(const glm::vec3& v);

        static glm::vec3 squareToUniformSphereCap(const glm::vec2& sample, float cosThetaMax);
        static float squareToUniformSphereCapPdf(float cosThetaMax);

        static glm::vec3 squareToCosineHemisphere(const glm::vec2& sample);
        static float squareToCosineHemispherePdf(const glm::vec3& v);

        static glm::vec3 squareToUniformTriangle(const glm::vec2& sample);

        static glm::vec3 squareToUniformCylinder(const glm::vec2& sample, float cosThetaMin, float cosThetaMax);

    private:
        static inline glm::vec3 cylinderToSphereSection(const glm::vec2& sample, float cosThetaMin, float cosThetaMax);

    };
}