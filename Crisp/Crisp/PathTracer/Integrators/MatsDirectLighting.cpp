#include "MatsDirectLighting.hpp"

#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Lights/Light.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp {
MatsDirectLightingIntegrator::MatsDirectLightingIntegrator(const VariantMap& /*params*/) {}

MatsDirectLightingIntegrator::~MatsDirectLightingIntegrator() {}

void MatsDirectLightingIntegrator::preprocess(pt::Scene* /*scene*/) {}

Spectrum MatsDirectLightingIntegrator::Li(
    const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*illumFlags*/) const {
    Intersection its;
    if (!scene->rayIntersect(ray, its)) {
        return Spectrum(0.0f);
    }

    Spectrum L(0.0f);

    if (its.shape->getLight()) {
        Light::Sample sample(ray.o, its.p, its.shFrame.n);
        L += its.shape->getLight()->eval(sample);
    }

    BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
    auto bsdfSpec = its.shape->getBSDF()->sample(bsdfSample, sampler);

    Intersection sampledRayIts;
    Ray3 sampledRay(its.p, its.toWorld(bsdfSample.wo));
    if (!scene->rayIntersect(sampledRay, sampledRayIts)) {
        return L;
    }

    if (!sampledRayIts.shape->getLight()) {
        return L;
    }

    auto light = sampledRayIts.shape->getLight();
    Light::Sample lightSample(its.p, sampledRayIts.p, sampledRayIts.shFrame.n);
    auto lightSpec = light->eval(lightSample);

    L += lightSpec * bsdfSpec;

    return L;
}
} // namespace crisp