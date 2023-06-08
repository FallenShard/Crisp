#pragma once

#include <Crisp/Math/Transform.hpp>
#include <Crisp/PathTracer/Cameras/Camera.hpp>

namespace crisp
{
class PerspectiveCamera : public Camera
{
public:
    PerspectiveCamera(const VariantMap& params = VariantMap());

    virtual Spectrum sampleRay(Ray3& ray, const glm::vec2& posSample, const glm::vec2& apertureSample) const;

private:
    glm::vec2 m_invImageSize;

    float m_fov;
    float m_nearZ;
    float m_farZ;

    Transform m_sampleToCamera;
    Transform m_cameraToWorld;
};
} // namespace crisp