#pragma once

#include <array>

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    class AbstractCamera
    {
    public:
        static constexpr uint32_t FrustumPlaneCount = 6;
        static constexpr uint32_t FrustumPointCount = 8;

        AbstractCamera();
        virtual ~AbstractCamera() = default;

        virtual void update(float dt) = 0;
        virtual void rotate(float dx, float dy) = 0;
        virtual void setRotation(float yaw, float pitch, float roll) = 0;

        virtual void walk(float dt) = 0;
        virtual void strafe(float dt) = 0;
        virtual void lift(float dt) = 0;

        void setupProjection(float fovY, float aspectRatio, float zNear, float zFar);
        void setupProjection(float fovY, float aspectRatio);
        const glm::mat4& getProjectionMatrix() const;

        void setFovY(float fovY);
        float getFovY() const;

        void setApectRatio(float aspectRatio);
        float getAspectRatio() const;

        void setNearPlaneDistance(float zNear);
        float getNearPlaneDistance() const;

        void setFarPlaneDistance(float zFar);
        float getFarPlaneDistance() const;

        void setPosition(const glm::vec3& position);
        glm::vec3 getPosition() const;

        virtual void setTarget(const glm::vec3& target) = 0;

        glm::vec3 getLookDirection() const;
        glm::vec3 getRightDirection() const;
        glm::vec3 getUpDirection() const;

        const glm::mat4& getViewMatrix() const;

        void calculateFrustumPlanes() const;
        bool isPointInFrustum(const glm::vec3& point);
        bool isSphereInFrustum(const glm::vec3& center, float radius);
        bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max);

        std::array<glm::vec4, FrustumPlaneCount> getFrustumPlanes() const;
        glm::vec4 getFrustumPlane(size_t index) const;

        std::array<glm::vec3, FrustumPointCount> getFrustumPoints(float zNear, float zFar) const;
        std::array<glm::vec3, FrustumPointCount> getFrustumPoints() const;

        glm::vec4 calculateBoundingSphere(float zNear, float zFar) const;

    protected:
        void updateProjectionMatrix();

        float m_fovY;
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

        std::array<glm::vec4, FrustumPlaneCount> m_frustumPlanes;
    };
}