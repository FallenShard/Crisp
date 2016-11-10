#include "PathTracer.hpp"

#include "Samplers/Sampler.hpp"
#include "Core/Scene.hpp"
#include "BSDFs/BSDF.hpp"
#include "Core/Intersection.hpp"
#include "Shapes/Shape.hpp"

namespace vesper
{
    PathTracerIntegrator::PathTracerIntegrator(const VariantMap& attribs)
    {
    }

    PathTracerIntegrator::~PathTracerIntegrator()
    {
    }

    void PathTracerIntegrator::preprocess(const Scene* scene)
    {
    }

    Spectrum PathTracerIntegrator::Li(const Scene* scene, Sampler& sampler, Ray3& ray) const
    {
        Spectrum L(0.0f);
        Spectrum throughput(1.0f);

        Intersection its;
        while (true)
        {
            if (!scene->rayIntersect(ray, its))
                return L;

            if (its.shape->getLight())
            {
                Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
                L += throughput * its.shape->getLight()->eval(lightSample);
            }

            BSDF::Sample bsdfSample(its.p, its.toLocal(-ray.d));
            throughput *= its.shape->getBSDF()->sample(bsdfSample, sampler);

            ray.o = its.p;
            ray.d = its.toWorld(bsdfSample.wo);
            ray.update();

            float q = 1.0f - std::min(throughput.maxCoeff(), 0.99f);
            if (sampler.next1D() > q)
                throughput /= 1.0f - q;
            else
                break;
        }

        return L;
    }
}