#include "EmsDirectLighting.hpp"

#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Lights/Light.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp
{
EmsDirectLightingIntegrator::EmsDirectLightingIntegrator(const VariantMap& /*params*/) {}

EmsDirectLightingIntegrator::~EmsDirectLightingIntegrator() {}

void EmsDirectLightingIntegrator::preprocess(pt::Scene* /*scene*/) {}

Spectrum EmsDirectLightingIntegrator::Li(
    const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*illumFlags*/) const
{
    Intersection its;
    if (!scene->rayIntersect(ray, its))
        return Spectrum(0.0f);

    Spectrum L(0.0f);

    // Lighting from intersected emitter
    if (its.shape->getLight())
    {
        Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
        L += its.shape->getLight()->eval(lightSample);
    }

    // Sample random light in the scene
    Light::Sample lightSample(its.p);
    auto lightSpec = scene->sampleLight(its, sampler, lightSample);

    // Check if light sample gives any contribution
    if (lightSpec.isZero() || scene->rayIntersect(lightSample.shadowRay))
        return L;

    // Evaluate the BSDF at the intersection
    BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
    bsdfSample.eta = 1.0f;
    bsdfSample.measure = BSDF::Measure::SolidAngle;
    auto bsdfSpec = its.shape->getBSDF()->eval(bsdfSample);

    L += lightSpec * bsdfSpec;

    return L;
}
} // namespace crisp