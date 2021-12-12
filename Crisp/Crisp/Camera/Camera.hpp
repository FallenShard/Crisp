#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <array>

namespace crisp
{
    struct CameraParameters
    {
        glm::mat4 V;
        glm::mat4 P;
        glm::vec2 screenSize;
        glm::vec2 nearFar;
    };

    class Camera
    {
    public:
        static constexpr uint32_t FrustumPlaneCount = 6;
        static constexpr uint32_t FrustumPointCount = 8;

        Camera(int32_t viewportWidth, int32_t viewportHeight);
        Camera(int32_t viewportWidth, int32_t viewportHeight, float zNear, float zFar);

        void setVerticalFov(float verticalFov);
        float getVerticalFov() const;
        void setViewportSize(int32_t viewportWidth, int32_t viewportHeight);
        void setAspectRatio(float aspectRatio);

        glm::mat4 getProjectionMatrix() const;

        void setPosition(const glm::vec3& position);
        void translate(const glm::vec3& translation);
        glm::vec3 getPosition() const;

        void setOrientation(const glm::dquat& orientation);
        glm::mat4 getViewMatrix() const;
        glm::mat4 getInvViewMatrix() const;

        glm::vec3 getRightDir() const;
        glm::vec3 getLookDir() const;
        glm::vec3 getUpDir() const;

        glm::vec2 getViewDepthRange() const;

        std::array<glm::vec3, Camera::FrustumPointCount> computeFrustumPoints(float zNear, float zFar) const;
        std::array<glm::vec3, Camera::FrustumPointCount> computeFrustumPoints() const;

        glm::vec4 computeFrustumBoundingSphere(float zNear, float zFar) const;

    private:
        void updateProjectionMatrix();
        void updateViewMatrix();

        float m_verticalFov;
        float m_aspectRatio;
        float m_zNear;
        float m_zFar;
        glm::mat4 m_P;

        glm::vec3 m_position;
        glm::dquat m_orientation;
        glm::mat4 m_V;
        glm::mat4 m_invV;

        std::array<glm::vec4, FrustumPlaneCount> m_frustumPlanes;
    };
}