#include "MisDirectLighting.hpp"

#include "Core/Intersection.hpp"
#include "Core/Scene.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"
#include "Shapes/Shape.hpp"
#include "BSDFs/BSDF.hpp"
#include "Lights/Light.hpp"

namespace vesper
{
    MisDirectLightingIntegrator::MisDirectLightingIntegrator(const VariantMap& params)
    {
    }

    MisDirectLightingIntegrator::~MisDirectLightingIntegrator()
    {
    }

    void MisDirectLightingIntegrator::preprocess(const Scene* scene)
    {
    }

    Spectrum MisDirectLightingIntegrator::Li(const Scene* scene, Sampler& sampler, Ray3& ray) const
    {
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Spectrum(0.0f);

        Spectrum L(0.0f);

        if (its.shape->getLight())
        {
            Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
            L += its.shape->getLight()->eval(lightSample);
        }

        L += lightImportanceSample(scene, sampler, ray, its);
        L += bsdfImportanceSample(scene, sampler, ray, its);

        return L;
    }

    Spectrum MisDirectLightingIntegrator::lightImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
    {
        Light::Sample lightSample(its.p);
        auto lightSpec = scene->sampleLight(its, sampler, lightSample);

        float cosFactor = glm::dot(its.shFrame.n, lightSample.wi);
        if (cosFactor <= 0.0f || lightSpec.isZero() || scene->rayIntersect(lightSample.shadowRay))
            return Spectrum(0.0f);

        BSDF::Sample bsdfSample(its.p, its.toLocal(-ray.d), its.toLocal(lightSample.wi), its.uv);
        bsdfSample.measure = BSDF::Measure::SolidAngle;
        bsdfSample.eta     = 1.0f;
        auto bsdfSpec = its.shape->getBSDF()->eval(bsdfSample);

        float pdfLight = lightSample.pdf;
        float pdfBsdf = its.shape->getBSDF()->pdf(bsdfSample);
        return pdfLight * bsdfSpec * lightSpec * cosFactor / (pdfLight + pdfBsdf);
    }

    Spectrum MisDirectLightingIntegrator::bsdfImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
    {
        BSDF::Sample bsdfSample(its.p, its.toLocal(-ray.d), its.uv);
        auto bsdfSpec = its.shape->getBSDF()->sample(bsdfSample, sampler);
        float pdfBsdf = its.shape->getBSDF()->pdf(bsdfSample);

        Intersection bsdfIts;
        Ray3 bsdfRay(its.p, its.toWorld(bsdfSample.wo));
        if (!scene->rayIntersect(bsdfRay, bsdfIts))
            return Spectrum(0.0f);

        if (!bsdfIts.shape->getLight())
            return Spectrum(0.0f);

        auto light = bsdfIts.shape->getLight();
        Light::Sample lightSam(its.p, bsdfIts.p, bsdfIts.shFrame.n);
        lightSam.wi = bsdfRay.d;
        auto lightSpec = light->eval(lightSam);

        float pdfLight = light->pdf(lightSam);

        return  bsdfSpec * lightSpec * pdfBsdf / (pdfBsdf + pdfLight);
    }
}