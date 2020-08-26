#include "DirectionalLight.hpp"

#include <CrispCore/Log.hpp>

namespace crisp
{
    namespace
    {
        glm::mat4 calculateViewMatrix(const glm::vec3& direction)
        {

            glm::vec3 up(0.0f, 1.0f, 0.0f);
            if (glm::dot(direction, up) > 0.999f)
                up = glm::vec3(0.0f, 0.0f, 1.0f);

            const glm::vec3 right = glm::normalize(glm::cross(direction, up));
            up = glm::normalize(glm::cross(right, direction));

            const glm::vec3 origin(0.0f, 0.0f, 0.0f);
            return glm::lookAt(origin, origin + direction, up);
        }

        glm::mat4 fitTightOrthoAroundFrustum(const glm::mat4& view, const std::array<glm::vec3, 8>& worldFrustumPoints)
        {
            glm::vec3 minCorner(std::numeric_limits<float>::max());
            glm::vec3 maxCorner(std::numeric_limits<float>::lowest());
            for (const auto& point : worldFrustumPoints)
            {
                glm::vec3 lightViewPoint = view * glm::vec4(point, 1.0f);
                minCorner = glm::min(minCorner, lightViewPoint);
                maxCorner = glm::max(maxCorner, lightViewPoint);
            }



            //glm::vec2 unitsPerTexel(0.0f);
            //
            //FLOAT fWorldUnitsPerTexel = fCascadeBound /
            //    (float)m_CopyOfCascadeConfig.m_iBufferSize;

            // Because view matrix looks down the -Z axis, maxZ value will be "behind" the minZ value
            // That's why we reverse them
            glm::vec3 orthoMin(minCorner.x, minCorner.y, -maxCorner.z);


            glm::vec3 orthoMax(maxCorner.x, maxCorner.y, -minCorner.z);

            //float unitsPerTexel = std::max(maxCorner.x - minCorner.x, maxCorner.y - minCorner.y);
            //unitsPerTexel /= 1024.0f;
            //orthoMin.x /= unitsPerTexel;
            //orthoMin.x = std::floor(orthoMin.x) * unitsPerTexel;
            //orthoMin.y /= unitsPerTexel;
            //orthoMin.y = std::floor(orthoMin.y) * unitsPerTexel;
            //orthoMax.x /= unitsPerTexel;
            //orthoMax.x = std::floor(orthoMax.x) * unitsPerTexel;
            //orthoMax.y /= unitsPerTexel;
            //orthoMax.y = std::floor(orthoMax.y) * unitsPerTexel;

            static int C = 0;
            if (C % 4 == 0)
                logDebug("Length {}, {}\n", orthoMax.x - orthoMin.x, orthoMax.y - orthoMin.y);
            C++;

            // zNear and zFar represent distances along the -Z axis from the viewer
            return glm::ortho(orthoMin.x, orthoMax.x, orthoMin.y, orthoMax.y, orthoMin.z, orthoMax.z);

            //auto t1 = m_projection * glm::vec4(orthoMin.x, orthoMin.y, -orthoMin.z, 1.0f); // should give [-1, -1, 0]
            //auto t2 = m_projection * glm::vec4(orthoMax.x, orthoMax.y, -orthoMax.z, 1.0f); // should give [ 1,  1, 1]
        }

        glm::mat4 fitSphereOrthoAroundFrustum(glm::mat4& view, const std::array<glm::vec3, 8>& worldFrustumPoints, const glm::vec3& direction)
        {
            static int C = 0;

            glm::vec3 minCorner(std::numeric_limits<float>::max());
            glm::vec3 maxCorner(std::numeric_limits<float>::lowest());



            glm::vec3 center(0.0f);
            for (const auto& point : worldFrustumPoints)
            {
                glm::vec3 lightViewPoint = /*view **/ glm::vec4(point, 1.0f);
                minCorner = glm::min(minCorner, lightViewPoint);
                maxCorner = glm::max(maxCorner, lightViewPoint);
                center += lightViewPoint;
            }
            center /= 8.0f;

            float radius = std::numeric_limits<float>::lowest();
            for (const auto& p : worldFrustumPoints)
                radius = std::max(radius, glm::length(center - p));


            //float x = std::ceil(glm::dot(center, up), )


            glm::vec3 extent = maxCorner - minCorner;
            float maxLength = std::max(extent.x, std::max(extent.y, extent.z));
            extent = glm::vec3(maxLength);

            // Because view matrix looks down the -Z axis, maxZ value will be "behind" the minZ value
            // That's why we reverse them
            const glm::vec3 orthoMin(-radius, -radius, center.y - radius);
            const glm::vec3 orthoMax(+radius, +radius, center.z + radius);

            if (C % 4 == 0)
                logDebug("Sphere Radius {}, Z Range: {}, {}\n", radius, orthoMin.z, orthoMax.z);
            C++;

            // zNear and zFar represent distances along the -Z axis from the viewer
            return glm::ortho(orthoMin.x, orthoMax.x, orthoMin.y, orthoMax.y, orthoMin.z, orthoMax.z);

            //auto t1 = m_projection * glm::vec4(orthoMin.x, orthoMin.y, -orthoMin.z, 1.0f); // should give [-1, -1, 0]
            //auto t2 = m_projection * glm::vec4(orthoMax.x, orthoMax.y, -orthoMax.z, 1.0f); // should give [ 1,  1, 1]
        }
    }

    DirectionalLight::DirectionalLight(const glm::vec3& direction, const glm::vec3& radiance, const glm::vec3& minCorner, const glm::vec3& maxCorner)
        : m_direction(glm::normalize(direction))
        , m_radiance(radiance)
    {
        m_view       = calculateViewMatrix(m_direction);
        m_projection = glm::ortho(minCorner.x, maxCorner.x, minCorner.y, maxCorner.y, minCorner.z, maxCorner.z);
    }

    void DirectionalLight::setDirection(glm::vec3 direction)
    {
        m_direction = glm::normalize(direction);
        m_view      = calculateViewMatrix(m_direction);
    }

    const glm::vec3& DirectionalLight::getDirection() const
    {
        return m_direction;
    }

    const glm::mat4& DirectionalLight::getViewMatrix() const
    {
        return m_view;
    }

    const glm::mat4& DirectionalLight::getProjectionMatrix() const
    {
        return m_projection;
    }

    void DirectionalLight::fitProjectionToFrustum(const std::array<glm::vec3, 8>& worldFrustumPoints)
    {
        //m_projection = fitSphereOrthoAroundFrustum(m_view, worldFrustumPoints);
        m_projection = fitSphereOrthoAroundFrustum(m_view, worldFrustumPoints, m_direction);
    }

    void DirectionalLight::fitProjectionToFrustum(const std::array<glm::vec3, 8>& worldFrustumPoints, const glm::vec3& center, float radius, uint32_t shadowMapSize)
    {
        static const glm::vec3 YAxis(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(m_direction, YAxis));
        const glm::vec3 up    = glm::normalize(glm::cross(right, m_direction));

        const float halfMapRes = static_cast<float>(shadowMapSize) / 2.0f;
        const float texelWorldScale = radius / halfMapRes;

        float x = std::ceil(glm::dot(center, up)    / texelWorldScale) * texelWorldScale;
        float y = std::ceil(glm::dot(center, right) / texelWorldScale) * texelWorldScale;
        glm::vec3 adjCenter = up * x + right * y + m_direction * glm::dot(center, m_direction);

        static constexpr float zOffset = 0.0f;
        m_projection = glm::ortho(-radius, radius, -radius, radius, zOffset, 2 * radius + zOffset);

        glm::vec3 origin = adjCenter - m_direction * (radius + zOffset);
        glm::vec3 target = adjCenter;

        m_view = glm::lookAt(origin, target, up);

        // Alternative solution: compute offset in image space and snap to integer

        // in post-projection space, but w = 1, so already in NDC
        //glm::vec4 originShadowSpace = m_projection * m_view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // in image space, [-texSize/2, texSize/2 - 1]
        //glm::vec2 originImageSpace(originShadowSpace.x * halfMapRes, originShadowSpace.y * halfMapRes);
        //m_projection[3][0] += (std::ceil(originImageSpace.x) - originImageSpace.x) / halfMapRes;
        //m_projection[3][1] += (std::ceil(originImageSpace.y) - originImageSpace.y) / halfMapRes;
    }

    LightDescriptor DirectionalLight::createDescriptor() const
    {
        LightDescriptor desc = {};
        desc.V         = m_view;
        desc.P         = m_projection;
        desc.VP        = m_projection * m_view;
        desc.direction = glm::vec4(-m_direction, 0.0f);
        desc.spectrum  = glm::vec4(m_radiance, 1.0f);
        return desc;
    }
}