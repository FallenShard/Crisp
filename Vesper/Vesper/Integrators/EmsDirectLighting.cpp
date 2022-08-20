#include "EmsDirectLighting.hpp"

#include "BSDFs/BSDF.hpp"
#include "Core/Intersection.hpp"
#include "Core/Scene.hpp"
#include "Lights/Light.hpp"
#include "Samplers/Sampler.hpp"
#include "Shapes/Shape.hpp"
#include <Crisp/Math/Warp.hpp>

namespace crisp
{
EmsDirectLightingIntegrator::EmsDirectLightingIntegrator(const VariantMap& /*params*/) {}

EmsDirectLightingIntegrator::~EmsDirectLightingIntegrator() {}

void EmsDirectLightingIntegrator::preprocess(Scene* /*scene*/) {}

Spectrum EmsDirectLightingIntegrator::Li(
    const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*illumFlags*/) const
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