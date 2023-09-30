#include "DirectLighting.hpp"

#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Lights/Light.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp {
DirectLightingIntegrator::DirectLightingIntegrator(const VariantMap& /*params*/) {}

DirectLightingIntegrator::~DirectLightingIntegrator() {}

void DirectLightingIntegrator::preprocess(pt::Scene* /*scene*/) {}

Spectrum DirectLightingIntegrator::Li(
    const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*illumFlags*/) const {
    Intersection its;
    if (!scene->rayIntersect(ray, its)) {
        return Spectrum(0.0f);
    }

    Spectrum L(0.0f);

    Light::Sample lightSample(its.p);
    auto lightSpec = scene->sampleLight(its, sampler, lightSample);

    if (lightSpec.isZero() || scene->rayIntersect(lightSample.shadowRay)) {
        return Spectrum(0.0f);
    }

    BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
    bsdfSample.measure = BSDF::Measure::SolidAngle;
    auto bsdfSpec = its.shape->getBSDF()->eval(bsdfSample);

    L += lightSpec * bsdfSpec;

    return L;
}
} // namespace crisp