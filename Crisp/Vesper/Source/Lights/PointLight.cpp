#include "PointLight.hpp"

#include <glm/gtx/norm.hpp>

#include "Math/Operations.hpp"

namespace vesper
{
    PointLight::PointLight(const VariantMap& params)
    {
        m_position = params.get<glm::vec3>("position", glm::vec3(0.0f, 3.0f, 0.0f));
        m_power = Spectrum(params.get<glm::vec3>("power", glm::vec3(500.0f)));
    }

    PointLight::~PointLight()
    {
    }

    Spectrum PointLight::eval(const Light::Sample& sample) const
    {
        return m_power * InvFourPI / glm::length2(m_position - sample.ref);
    }

    Spectrum PointLight::sample(Light::Sample& sample, Sampler& sampler) const
    {
        sample.p = m_position;
        sample.pdf = 1.0f;
        sample.wi = m_position - sample.ref;

        float squaredDist = glm::dot(sample.wi, sample.wi);
        sample.wi = glm::normalize(sample.wi);
        sample.shadowRay = Ray3(sample.ref, sample.wi, Ray3::Epsilon, sqrtf(squaredDist));

        return m_power * InvFourPI / squaredDist;
    }

    float PointLight::pdf(const Light::Sample& sample) const
    {
        return 0.0f;
    }

    Spectrum PointLight::samplePhoton(Ray3& ray, Sampler& sampler) const
    {
        return Spectrum();
    }
}