#pragma once

#include "Light.hpp"

namespace vesper
{
    class PointLight : public Light
    {
    public:
        PointLight(const VariantMap& params = VariantMap());
        ~PointLight();

        virtual Spectrum eval(const Light::Sample& emitterSample) const override;
        virtual Spectrum sample(Light::Sample& emitterSample, Sampler& sampler) const override;
        virtual float pdf(const Light::Sample& emitterSample) const override;

        virtual Spectrum samplePhoton(Ray3& ray, Sampler& sampler) const override;
        virtual bool isDelta() const override;

    private:
        glm::vec3 m_position;
        Spectrum m_power;
    };
}