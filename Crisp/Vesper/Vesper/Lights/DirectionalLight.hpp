#pragma once

#include "Light.hpp"

namespace vesper
{
    class DirectionalLight : public Light
    {
    public:
        DirectionalLight(const VariantMap& params = VariantMap());
        ~DirectionalLight();

        virtual Spectrum eval(const Light::Sample& emitterSample) const override;
        virtual Spectrum sample(Light::Sample& emitterSample, Sampler& sampler) const override;
        virtual float pdf(const Light::Sample& emitterSample) const override;

        virtual Spectrum samplePhoton(Ray3& ray, Sampler& sampler) const override;

    private:
        glm::vec3 m_direction;
        Spectrum m_power;
    };
}