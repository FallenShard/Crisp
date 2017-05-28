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

        virtual bool update(float dt) = 0;
        virtual void rotate(float dx, float dy) = 0;
        
        virtual void walk(float dt) = 0;
        virtual void strafe(float dt) = 0;
        virtual void lift(float dt) = 0;

        void setupProjection(float fovY, float aspectRatio, float zNear = 0.1f, float zFar = 1000.0f);
        const glm::mat4& getProjectionMatrix() const;

        void setFov(float fovY);
        float getFov() const;

        void setApectRatio(float aspectRatio);
        float getAspectRatio() const;

        void setNearPlaneDistance(float zNear);
        float getNearPlaneDistance() const;

        void setFarPlaneDistance(float zFar);
        float getFarPlaneDistance() const;

        void setPosition(const glm::vec3& position);
        glm::vec3 getPosition() const;

        glm::vec3 getLookDirection() const;
        glm::vec3 getRightDirection() const;
        glm::vec3 getUpDirection() const;

        const glm::mat4& getViewMatrix() const;

        void calculateFrustumPlanes() const;
        bool isPointInFrustum(const glm::vec3& point);
        bool isSphereInFrustum(const glm::vec3& center, float radius);
        bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max);

        std::array<glm::vec4, NumFrustumPlanes> getFrustumPlanes() const;
        glm::vec4 getFrustumPlane(size_t index) const;

        std::array<glm::vec3, 8> getFrustumPoints(float zNear, float zFar) const;

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

        glm::vec3 m_absPos;
        glm::vec3 m_absTarget;

        bool m_recalculateViewMatrix;

        std::array<glm::vec4, NumFrustumPlanes> m_frustumPlanes;
    };
}