#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Perspective.hpp"

#include "Math/Operations.hpp"

#include "ReconstructionFilters/GaussianFilter.hpp"

namespace vesper
{
    PerspectiveCamera::PerspectiveCamera(const VariantMap& params)
    {
        m_imageSize = params.get<glm::ivec2>("imageSize", {960, 540});
        m_fov = params.get<float>("fov", 70.0f);
        m_nearZ = params.get<float>("zNear", 1e-4f);
        m_farZ = params.get<float>("zFar", 1e4f);

        m_invImageSize.x = 1.0f / m_imageSize.x;
        m_invImageSize.y = 1.0f / m_imageSize.y;

        auto position = params.get<glm::vec3>("position", {0, 0, 5});
        auto target   = params.get<glm::vec3>("target", {0, 0, 0});
        auto up       = params.get<glm::vec3>("up", {0, 1, 0});
        m_cameraToWorld = Transform(glm::lookAt(position, target, up)).invert();

        float aspect = m_imageSize.x / static_cast<float>(m_imageSize.y);

        Transform ndcScale     = Transform::createScale(glm::vec3(2.0f, 2.0f / aspect, 1.0f));
        Transform screenShift  = Transform::createTranslation(glm::vec3(-1.0f, -1.0f / aspect, 0.0f));
        Transform unprojection = Transform(glm::perspective(glm::radians(m_fov), 1.0f, m_nearZ, m_farZ)).invert();
        m_sampleToCamera = unprojection * screenShift * ndcScale;

        if (!m_filter)
        {
            m_filter = std::make_unique<GaussianFilter>();
        }
    }

    Spectrum PerspectiveCamera::sampleRay(Ray3& ray, const glm::vec2& posSample, const glm::vec2& apertureSample) const
    {
        glm::vec3 screenPos = m_sampleToCamera.transformPoint(glm::vec3(posSample * m_invImageSize, 0.0f));
        glm::vec3 rayDir = glm::normalize(screenPos);
        float invZ = -1.0f / rayDir.z;

        ray.o = m_cameraToWorld.transformPoint(glm::vec3(0.0f));
        ray.d = m_cameraToWorld.transformDir(rayDir);
        ray.minT = m_nearZ * invZ;
        ray.maxT = m_farZ * invZ;
        ray.update();

        return Spectrum(1.0f);
    }
}