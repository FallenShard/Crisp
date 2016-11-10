#include "Warp.hpp"

#include "Constants.hpp"

namespace vesper
{
    glm::vec2 Warp::squareToUniformSquare(const glm::vec2& sample)
    {
        return sample;
    }

    float Warp::squareToUniformSquarePdf()
    {
        return 1.0f;
    }

    glm::vec2 Warp::squareToUniformDisk(const glm::vec2& sample)
    {
        float r = sqrtf(sample.y);
        float theta = 2.0f * PI * sample.x;
        return glm::vec2(r * cosf(theta), r * sinf(theta));
    }

    float Warp::squareToUniformDiskPdf()
    {
        return InvPI;
    }

    glm::vec3 Warp::squareToUniformSphere(const glm::vec2& sample)
    {
        return cylinderToSphereSection(sample, 1.0f, -1.0f);
    }

    float Warp::squareToUniformSpherePdf()
    {
        return InvFourPI;
    }

    glm::vec3 Warp::squareToUniformHemisphere(const glm::vec2& sample)
    {
        return cylinderToSphereSection(sample, 1.0f, 0.0f);
    }

    float Warp::squareToUniformHemispherePdf(const glm::vec3& v)
    {
        return InvTwoPI;
    }

    glm::vec3 Warp::squareToUniformSphereCap(const glm::vec2& sample, float cosThetaMax)
    {
        return cylinderToSphereSection(sample, 1.0f, cosThetaMax);
    }

    float Warp::squareToUniformSphereCapPdf(const glm::vec3& v, float cosThetaMax)
    {
        return InvTwoPI / (1.0f - cosThetaMax);
    }

    glm::vec3 Warp::squareToCosineHemisphere(const glm::vec2& sample)
    {
        float radius = sqrtf(sample.y);
        float theta = 2.0f * PI * sample.x;
        float x = radius * cosf(theta);
        float y = radius * sinf(theta);
        return glm::vec3(x, y, 1.0f - x * x - y * y);
    }

    float Warp::squareToCosineHemispherePdf(const glm::vec3& v)
    {
        return v.z * InvPI;
    }

    glm::vec3 Warp::squareToBeckmann(const glm::vec2& sample, float alpha)
    {
        float denominator = 1.0f - alpha * alpha * log(1.0f - sample.y);
        float cosTheta = sqrtf(fabs(1.0f / denominator));
        float theta = acosf(cosTheta);
        float phi = 2.0f * PI * sample.x;
        float sinTheta = sinf(theta);

        return glm::vec3(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
    }

    float Warp::squareToBeckmannPdf(const glm::vec3& m, float alpha)
    {
        float cosTheta2 = m.z * m.z;
        float alpha2 = alpha * alpha;

        return exp((cosTheta2 - 1.0f) / (cosTheta2 * alpha2)) / (PI * alpha2 * cosTheta2 * m.z);
    }

    glm::vec3 Warp::squareToUniformTriangle(const glm::vec2& sample)
    {
        float val = sqrtf(sample.x);
        float u = 1.0f - val;
        float v = sample.y * val;
        return glm::vec3(u, v, 1.f - u - v);
    }

    glm::vec3 Warp::squareToUniformCylinder(const glm::vec2& sample, float cosThetaMin, float cosThetaMax)
    {
        float z = cosThetaMin + sample.y * (cosThetaMax - cosThetaMin);
        float phi = 2.0f * PI * sample.x;

        return glm::vec3(cosf(phi), sinf(phi), z);
    }

    glm::vec3 Warp::cylinderToSphereSection(const glm::vec2& sample, float cosThetaMin, float cosThetaMax)
    {
        glm::vec3 cylinderPt = squareToUniformCylinder(sample, cosThetaMin, cosThetaMax);
        float radius = sqrtf(1.0f - cylinderPt.z * cylinderPt.z);
        return glm::vec3(cylinderPt.x * radius, cylinderPt.y * radius, cylinderPt.z);
    }
}