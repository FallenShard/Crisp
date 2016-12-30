#pragma once

#include <array>

#include "Math/Headers.hpp"
#include <glm/gtx/quaternion.hpp>

namespace crisp
{
    class AbstractCamera
    {
    public:
        static constexpr size_t NumFrustumPlanes = 6;

        AbstractCamera();
        virtual ~AbstractCamera();

        virtual void update(float dt) = 0;

        void setupProjection(float fovY, float aspectRatio, float zNear = 1.0f, float zFar = 100.0f);
        
        void setPosition(const glm::vec3& position);
        glm::vec3 getPosition() const;

        glm::vec3 getLookDirection() const;
        glm::vec3 getRightDirection() const;
        glm::vec3 getUpDirection() const;

        void setFov(float fovY);
        float getFov() const;

        void setApectRatio(float aspectRatio);
        float getAspectRatio() const;

        void calculateFrustumPlanes() const;
        bool isPointInFrustum(const glm::vec3& point);
        bool isSphereInFrustum(const glm::vec3& center, float radius);
        bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max);

        const glm::mat4& getViewMatrix() const;
        const glm::mat4& getProjectionMatrix() const;

        std::array<glm::vec4, NumFrustumPlanes> getFrustumPlanes() const;
        glm::vec4 getFrustumPlane(size_t index) const;

    protected:
        float m_fov;
        float m_aspectRatio;
        float m_zNear;
        float m_zFar;
        glm::mat4 m_P;

        
        glm::vec3 m_position;
        glm::vec3 m_look;
        glm::vec3 m_right;
        glm::vec3 m_up;
        glm::mat4 m_V;

        std::array<glm::vec4, NumFrustumPlanes> m_frustumPlanes;
    };
}