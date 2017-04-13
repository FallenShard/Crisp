#include "DirectLighting.hpp"

#include "Core/Intersection.hpp"
#include "Core/Scene.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"
#include "Shapes/Shape.hpp"
#include "BSDFs/BSDF.hpp"
#include "Lights/Light.hpp"

namespace vesper
{
    DirectLightingIntegrator::DirectLightingIntegrator(const VariantMap& params)
    {
    }

    DirectLightingIntegrator::~DirectLightingIntegrator()
    {
    }

    void DirectLightingIntegrator::preprocess(const Scene* scene)
    {
    }

    Spectrum DirectLightingIntegrator::Li(const Scene* scene, Sampler& sampler, Ray3& ray) const
    {
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Spectrum(0.0f);

        Spectrum L(0.0f);

        Light::Sample lightSample(its.p);
        auto lightSpec = scene->sampleLight(its, sampler, lightSample);

        if (lightSpec.isZero() || scene->rayIntersect(lightSample.shadowRay))
            return Spectrum(0.0f);

        BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
        bsdfSample.measure = BSDF::Measure::SolidAngle;
        auto bsdfSpec = its.shape->getBSDF()->eval(bsdfSample);

        L += lightSpec * bsdfSpec;

        return L;
    }
}