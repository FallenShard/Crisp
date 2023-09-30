#include "AreaLight.hpp"

#include <Crisp/Math/Operations.hpp>
#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp {
AreaLight::AreaLight(const VariantMap& params) {
    m_radiance = params.get("radiance", Spectrum(10.0f));
}

Spectrum AreaLight::eval(const Light::Sample& sample) const {
    glm::vec3 wi = sample.p - sample.ref;
    float cosTheta = glm::dot(sample.n, glm::normalize(-wi));

    if (cosTheta <= 0.0f) {
        return Spectrum(0.0f);
    }

    return m_radiance;
}

Spectrum AreaLight::sample(Light::Sample& sample, Sampler& sampler) const {
    Shape::Sample shapeSample(sample.ref);
    m_shape->sampleSurface(shapeSample, sampler);

    sample.p = shapeSample.p;
    sample.n = shapeSample.n;
    sample.wi = sample.p - sample.ref;
    float squaredDist = glm::dot(sample.wi, sample.wi);
    sample.wi = glm::normalize(sample.wi);
    sample.shadowRay = Ray3(sample.ref, sample.wi, Ray3::Epsilon, std::sqrtf(squaredDist) - Ray3::Epsilon);

    float cosTheta = glm::dot(sample.n, -sample.wi);
    Spectrum radiance(0.0f);

    if (cosTheta <= 0.0f) {
        sample.pdf = 0.0f;
    } else {
        sample.pdf = shapeSample.pdf * squaredDist / cosTheta;
        radiance = m_radiance / sample.pdf;
    }

    return radiance;
}

float AreaLight::pdf(const Light::Sample& sample) const {
    Shape::Sample shapeSample(sample.ref);
    float areaPdf = m_shape->pdfSurface(shapeSample);

    float cosTheta = glm::dot(sample.n, -sample.wi);
    if (cosTheta <= 0.0f) {
        return 0.0f;
    }

    glm::vec3 wi = sample.p - sample.ref;
    float squaredDist = glm::dot(wi, wi);
    return areaPdf * squaredDist / cosTheta;
}

Spectrum AreaLight::samplePhoton(Ray3& ray, Sampler& sampler) const {
    Shape::Sample shapeSample;
    m_shape->sampleSurface(shapeSample, sampler);

    glm::vec3 dir = warp::squareToCosineHemisphere(sampler.next2D());
    CoordinateFrame frame(shapeSample.n);

    ray = Ray3(shapeSample.p, frame.toWorld(dir));

    return PI<> * m_radiance / m_shape->pdfSurface(shapeSample);
}

bool AreaLight::isDelta() const {
    return false;
}
} // namespace crisp