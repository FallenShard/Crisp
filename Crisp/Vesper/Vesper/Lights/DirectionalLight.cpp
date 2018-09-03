#include "DirectionalLight.hpp"

#include <glm/gtx/norm.hpp>

#include <CrispCore/Math/Operations.hpp>

namespace crisp
{
    DirectionalLight::DirectionalLight(const VariantMap& params)
    {
        m_direction = params.get<glm::vec3>("direction", glm::vec3(1.0f, -1.0f, 0.0f));
        m_power = params.get("power", Spectrum(500.0f));
    }

    DirectionalLight::~DirectionalLight()
    {
    }

    Spectrum DirectionalLight::eval(const Light::Sample& sample) const
    {
        return m_power;
    }

    Spectrum DirectionalLight::sample(Light::Sample& sample, Sampler& sampler) const
    {
        sample.p = -m_direction * 1000.0f;
        sample.pdf = 1.0f;
        sample.wi = -m_direction;
        sample.shadowRay = Ray3(sample.ref, sample.wi, Ray3::Epsilon);

        return m_power;
    }

    float DirectionalLight::pdf(const Light::Sample& sample) const
    {
        return 0.0f;
    }

    Spectrum DirectionalLight::samplePhoton(Ray3& ray, Sampler& sampler) const
    {
        return Spectrum();
    }

    bool DirectionalLight::isDelta() const
    {
        return true;
    }
}