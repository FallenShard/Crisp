#pragma once

#include <Crisp/PathTracer/Lights/Light.hpp>

namespace crisp {
class AreaLight : public Light {
public:
    AreaLight(const VariantMap& params = VariantMap());

    virtual Spectrum eval(const Light::Sample& emitterSample) const override;
    virtual Spectrum sample(Light::Sample& emitterSample, Sampler& sampler) const override;
    virtual float pdf(const Light::Sample& emitterSample) const override;

    virtual Spectrum samplePhoton(Ray3& ray, Sampler& sampler) const override;
    virtual bool isDelta() const override;

private:
    Spectrum m_radiance;
};
} // namespace crisp