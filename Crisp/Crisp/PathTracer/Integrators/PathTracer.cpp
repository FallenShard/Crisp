#include "PathTracer.hpp"

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/BSSRDFs/BSSRDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp
{
PathTracerIntegrator::PathTracerIntegrator(const VariantMap& /*attribs*/) {}

PathTracerIntegrator::~PathTracerIntegrator() {}

void PathTracerIntegrator::preprocess(pt::Scene* /*scene*/) {}

Spectrum PathTracerIntegrator::Li(
    const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*illumFlags*/) const
{
    Spectrum L(0.0f);
    Spectrum throughput(1.0f);

    Intersection its;
    unsigned int numBounces = 0;
    while (true)
    {
        if (!scene->rayIntersect(ray, its))
        {
            L += throughput * scene->evalEnvLight(ray);
            return L;
        }

        if (its.shape->getLight())
        {
            Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
            L += throughput * its.shape->getLight()->eval(lightSample);
        }

        BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
        throughput *= its.shape->getBSDF()->sample(bsdfSample, sampler);

        ray = Ray3(its.p, its.toWorld(bsdfSample.wo));

        numBounces++;

        if (numBounces > 5)
        {
            float q = 1.0f - std::min(throughput.maxCoeff(), 0.99f);
            if (sampler.next1D() > q)
                throughput /= 1.0f - q;
            else
                break;
        }
    }

    return L;
}
} // namespace crisp