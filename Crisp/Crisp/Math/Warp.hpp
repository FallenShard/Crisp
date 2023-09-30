#pragma once

#include "Headers.hpp"

namespace crisp {
namespace warp {
glm::vec2 squareToUniformSquare(const glm::vec2& sample);
float squareToUniformSquarePdf();

glm::vec2 squareToUniformDisk(const glm::vec2& sample);
float squareToUniformDiskPdf();

glm::vec3 squareToUniformSphere(const glm::vec2& sample);
float squareToUniformSpherePdf();

glm::vec3 squareToUniformHemisphere(const glm::vec2& sample);
float squareToUniformHemispherePdf(const glm::vec3& v);

glm::vec3 squareToUniformSphereCap(const glm::vec2& sample, float cosThetaMax);
float squareToUniformSphereCapPdf(float cosThetaMax);

glm::vec3 squareToCosineHemisphere(const glm::vec2& sample);
float squareToCosineHemispherePdf(const glm::vec3& v);

glm::vec3 squareToUniformTriangle(const glm::vec2& sample);
} // namespace warp
} // namespace crisp