#include "CascadedShadowMapper.hpp"

#include "Camera/AbstractCamera.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/Pipelines/ShadowMapPipelines.hpp"
#include "vulkan/VulkanImage.hpp"

namespace crisp
{
    namespace
    {
        glm::mat4 computeSphereBoundTransform(const glm::vec3& dir, const glm::vec3& right, const glm::vec3& up, const std::array<glm::vec3, 8>& frustumPoints, uint32_t shadowMapSize,
            glm::vec3& orthoMin, glm::vec3& orthoMax)
        {
            glm::vec3 center(0.0f);
            for (const auto& point : frustumPoints)
                center += point;
            center /= static_cast<float>(frustumPoints.size());

            // Circumscribe a sphere around the frustum
            float radius = 0.0f;
            for (const auto& point : frustumPoints)
            {
                float distance = glm::length(point - center);
                radius = glm::max(radius, distance);
            }

            float magic = 16.0f;
            radius = std::ceil(radius * magic) / magic;

            float x = std::ceil(glm::dot(center, up) * shadowMapSize / radius) * radius / shadowMapSize;
            float y = std::ceil(glm::dot(center, right) * shadowMapSize / radius) * radius / shadowMapSize;
            center = up * x + right * y + dir * glm::dot(center, dir);

            glm::mat4 proj = glm::ortho(-radius, radius, -radius, radius, 0.0f, 2.0f * radius);
            glm::mat4 view = glm::lookAt(center - dir * radius, center, up);
            glm::mat4 lvp = proj * view;

            orthoMin = glm::vec3(-radius, -radius, 0.0f);
            orthoMax = glm::vec3(+radius, +radius, 2.0f * radius);

            glm::vec4 lightSpaceOrigin = lvp * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            lightSpaceOrigin *= shadowMapSize * 0.5f;
            glm::vec4 fractionalOffset = glm::vec4(round(lightSpaceOrigin.x), round(lightSpaceOrigin.y), 0.0f, 0.0f) - lightSpaceOrigin;
            fractionalOffset *= 2.0f / shadowMapSize;
            fractionalOffset.z = 0.0f;
            fractionalOffset.w = 0.0f;

            lvp[3] += fractionalOffset;
            return lvp;
        }

        //glm::mat4 computeTightBoundTransform(const glm::mat4& lightView, const std::array<glm::vec3, 8>& frustumPoints)
        //{
        //    glm::vec3 minCorner(std::numeric_limits<float>::max());
        //    glm::vec3 maxCorner(std::numeric_limits<float>::min());
        //
        //    for (const auto& point : frustumPoints)
        //    {
        //        glm::vec3 lightViewPoint = lightView * glm::vec4(point, 1.0f);
        //        minCorner = glm::min(minCorner, lightViewPoint);
        //        maxCorner = glm::max(maxCorner, lightViewPoint);
        //    }
        //
        //    float sx = 2.0f / (maxCorner.x - minCorner.x);
        //    float sy = 2.0f / (maxCorner.y - minCorner.y);
        //
        //    float scaleQuantizer = 64.0f;
        //    // quantize scale
        //    sx = 1.0f / std::ceil(1.0f / sx * scaleQuantizer) * scaleQuantizer;
        //    sy = 1.0f / std::ceil(1.0f / sy * scaleQuantizer) * scaleQuantizer;
        //
        //    float halfTexSize = static_cast<float>(texSize) * 0.5f;
        //    float ox = -0.5f * (maxCorner.x + minCorner.x) * sx;
        //    float oy = -0.5f * (maxCorner.y + minCorner.y) * sy;
        //    ox = std::ceil(ox * halfTexSize) / halfTexSize;
        //    oy = std::ceil(oy * halfTexSize) / halfTexSize;
        //
        //    glm::mat4 crop;
        //    crop[0][0] = sx;
        //    crop[1][1] = sy;
        //    crop[3][0] = ox;
        //    crop[3][1] = oy;
        //
        //    return {};
        //}

        glm::mat4 computeSimple(const glm::mat4& lightView, const std::array<glm::vec3, 8>& frustumPoints, glm::vec3& orthoMin, glm::vec3& orthoMax)
        {
            glm::vec3 minCorner(std::numeric_limits<float>::max());
            glm::vec3 maxCorner(std::numeric_limits<float>::lowest());

            for (const auto& point : frustumPoints)
            {
                glm::vec3 lightViewPoint = lightView * glm::vec4(point, 1.0f);
                minCorner = glm::min(minCorner, lightViewPoint);
                maxCorner = glm::max(maxCorner, lightViewPoint);
            }

            glm::vec3 center = (maxCorner + minCorner) / 2.0f;
            glm::vec3 dims = maxCorner - minCorner;
            float side = std::max(dims.x, dims.y) / 2.0f;

            orthoMin = glm::vec3(center.x - side, center.y - side, minCorner.z);
            orthoMax = glm::vec3(center.x + side, center.y + side, maxCorner.z);

            return glm::ortho(orthoMin.x, orthoMax.x, orthoMin.y, orthoMax.y, orthoMin.z, orthoMax.z) * lightView;
        }
    }

    CascadedShadowMapper::CascadedShadowMapper(Renderer* renderer, uint32_t numCascades, DirectionalLight light, float zFar, uint32_t shadowMapSize)
        : m_numCascades(numCascades)
        , m_light(light)
        , m_zFar(zFar)
        , m_shadowMapSize(shadowMapSize)
        , m_lightTransforms(numCascades)
        , m_lightTransformBuffer(std::make_unique<UniformBuffer>(renderer, numCascades * sizeof(glm::mat4), BufferUpdatePolicy::PerFrame))
        , m_lambda(0.5f)
        , m_splitBuffer(std::make_unique<UniformBuffer>(renderer, sizeof(SplitIntervals), BufferUpdatePolicy::PerFrame))
    {
        m_boundingBoxExtents.resize(m_numCascades);
        for (auto& bb : m_boundingBoxExtents)
            bb.resize(2);
    }

    CascadedShadowMapper::~CascadedShadowMapper()
    {
    }

    UniformBuffer* CascadedShadowMapper::getLightTransformBuffer() const
    {
        return m_lightTransformBuffer.get();
    }

    UniformBuffer* CascadedShadowMapper::getSplitBuffer() const
    {
        return m_splitBuffer.get();
    }

    glm::mat4 CascadedShadowMapper::getLightTransform(uint32_t cascadeIndex) const
    {
        return m_lightTransforms[cascadeIndex];
    }

    void CascadedShadowMapper::setSplitLambda(float lambda, const AbstractCamera& camera)
    {
        m_lambda = lambda;
        recalculateLightProjections(camera);
    }

    void CascadedShadowMapper::recalculateLightProjections(const AbstractCamera& camera)
    {
        float zNear = camera.getNearPlaneDistance();
        float range = m_zFar - zNear;
        float ratio = m_zFar / zNear;

        m_splitIntervals.lo[0] = zNear;
        m_splitIntervals.hi[m_numCascades - 1] = m_zFar;
        for (uint32_t i = 0; i < m_numCascades - 1; i++)
        {
            float p = (i + 1) / static_cast<float>(m_numCascades);
            float logSplit = zNear * std::pow(ratio, p);
            float linSplit = zNear + range * p;
            float splitPos = m_lambda * (logSplit - linSplit) + linSplit;
            m_splitIntervals.hi[i] = splitPos - zNear;
            m_splitIntervals.lo[i + 1] = m_splitIntervals.hi[i];
        }

        //float x = std::cosf(m_z);
        //float z = std::sinf(m_z);
        //glm::vec3 dir = glm::normalize(glm::vec3(x, -1.0f, z));// m_light.getDirection();
        glm::vec3 dir = m_light.getDirection();
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(dir, up));
        up = glm::normalize(glm::cross(right, dir));

        glm::vec3 origin = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::mat4 genericView = glm::lookAt(origin, origin + dir, up);

        for (uint32_t i = 0; i < m_numCascades; i++)
        {
            auto frustumPoints = camera.getFrustumPoints(m_splitIntervals.lo[i], m_splitIntervals.hi[i]);
            m_lightTransforms[i] = computeSimple(genericView, frustumPoints, m_boundingBoxExtents[i][0], m_boundingBoxExtents[i][1]);
            m_lightTransforms[i] = computeSphereBoundTransform(dir, right, up, frustumPoints, m_shadowMapSize, m_boundingBoxExtents[i][0], m_boundingBoxExtents[i][1]);
        }

        m_splitBuffer->updateStagingBuffer(&m_splitIntervals, sizeof(m_splitIntervals));
        m_lightTransformBuffer->updateStagingBuffer(m_lightTransforms.data(), m_lightTransforms.size() * sizeof(glm::mat4));
    }
}
