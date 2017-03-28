#pragma once

#include <vector>
#include <memory>

#include "Light.hpp"
#include "Math/Distribution1D.hpp"
#include "Core/MipMap.hpp"

namespace vesper
{
    class EnvironmentLight : public Light
    {
    public:
        EnvironmentLight(const VariantMap& params = VariantMap());
        ~EnvironmentLight();

        virtual Spectrum eval(const Light::Sample& emitterSample) const override;
        virtual Spectrum sample(Light::Sample& emitterSample, Sampler& sampler) const override;
        virtual float pdf(const Light::Sample& emitterSample) const override;

        virtual Spectrum samplePhoton(Ray3& ray, Sampler& sampler) const override;

        virtual void setBoundingSphere(const glm::vec4& sphereParams) override;

    private:
        std::unique_ptr<MipMap<Spectrum>> m_probe;
        float m_scale;

        std::vector<Distribution1D> m_phiPdfs;
        Distribution1D m_thetaPdf;

        glm::vec3 m_sceneCenter;
        float m_sceneRadius;
    };
}