#pragma once

#include "Light.hpp"

namespace vesper
{
    class AreaLight : public Light
    {
    public:
        AreaLight(const VariantMap& params = VariantMap());
        ~AreaLight();

        virtual Spectrum eval(const Light::Sample& emitterSample) const override;
        virtual Spectrum sample(Light::Sample& emitterSample, Sampler& sampler) const override;
        virtual float pdf(const Light::Sample& emitterSample) const override;

        virtual Spectrum samplePhoton(Ray3& ray, Sampler& sampler) const override;
        virtual bool isDelta() const override;

    private:
        glm::vec3 m_position;
        Spectrum m_radiance;
    };
}