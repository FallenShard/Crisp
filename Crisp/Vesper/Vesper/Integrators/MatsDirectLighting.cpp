#include "MatsDirectLighting.hpp"

#include "Core/Intersection.hpp"
#include "Core/Scene.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"
#include "Shapes/Shape.hpp"
#include "BSDFs/BSDF.hpp"
#include "Lights/Light.hpp"

namespace vesper
{
    MatsDirectLightingIntegrator::MatsDirectLightingIntegrator(const VariantMap& params)
    {
    }

    MatsDirectLightingIntegrator::~MatsDirectLightingIntegrator()
    {
    }

    void MatsDirectLightingIntegrator::preprocess(const Scene* scene)
    {
    }

    Spectrum MatsDirectLightingIntegrator::Li(const Scene* scene, Sampler& sampler, Ray3& ray) const
    {
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Spectrum(0.0f);

        Spectrum L(0.0f);

        if (its.shape->getLight())
        {
            Light::Sample sample(ray.o, its.p, its.shFrame.n);
            L += its.shape->getLight()->eval(sample);
        }

        BSDF::Sample bsdfSample(its.p, its.toLocal(-ray.d), its.uv);
        auto bsdfSpec = its.shape->getBSDF()->sample(bsdfSample, sampler);

        Intersection sampledRayIts;
        Ray3 sampledRay(its.p, its.toWorld(bsdfSample.wo));
        if (!scene->rayIntersect(sampledRay, sampledRayIts))
            return L;

        if (!sampledRayIts.shape->getLight())
            return L;

        auto light = sampledRayIts.shape->getLight();
        Light::Sample lightSample(its.p, sampledRayIts.p, sampledRayIts.shFrame.n);
        auto lightSpec = light->eval(lightSample);

        L += lightSpec * bsdfSpec;

        return L;
    }
}