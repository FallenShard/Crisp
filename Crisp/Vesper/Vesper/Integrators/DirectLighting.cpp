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

        if (its.shape->getLight())
        {
            Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
            L += its.shape->getLight()->eval(lightSample);
        }
        
        L += lightImportanceSample(scene, sampler, ray, its);
        L += bsdfImportanceSample(scene, sampler, ray, its);

        return L;
    }

    Spectrum DirectLightingIntegrator::lightImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
    {
        Light::Sample lightSample(its.p);
        auto lightSpec = scene->sampleLight(its, sampler, lightSample);
        
        float cosFactor = glm::dot(its.shFrame.n, lightSample.wi);
        if (cosFactor <= 0.0f || lightSpec.isZero() || scene->rayIntersect(lightSample.shadowRay))
            return Spectrum(0.0f);

        BSDF::Sample bsdfSample(its.p, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
        bsdfSample.measure = BSDF::Measure::SolidAngle;
        auto bsdfSpec = its.shape->getBSDF()->eval(bsdfSample);
        
        float pdfLight = lightSample.pdf;
        float pdfBsdf = its.shape->getBSDF()->pdf(bsdfSample);
        return pdfLight * bsdfSpec * lightSpec * cosFactor / (pdfLight + pdfBsdf);
    }

    Spectrum DirectLightingIntegrator::bsdfImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
    {
        BSDF::Sample bsdfSam(its.p, its.toLocal(-ray.d));
        bsdfSam.measure = BSDF::Measure::SolidAngle;
        auto bsdfSpec = its.shape->getBSDF()->sample(bsdfSam, sampler);
        
        Intersection bsdfIts;
        Ray3 bsdfRay(its.p, its.toWorld(bsdfSam.wo));
        if (!scene->rayIntersect(bsdfRay, bsdfIts))
            return Spectrum(0.0f);
        
        if (!bsdfIts.shape->getLight())
            return Spectrum(0.0f);

        auto light = bsdfIts.shape->getLight();
        Light::Sample lightSam(its.p, bsdfIts.p, bsdfIts.shFrame.n);
        lightSam.wi = bsdfRay.d;
        auto lightSpec = light->eval(lightSam);
        
        float pdfBsdf = its.shape->getBSDF()->pdf(bsdfSam);
        float pdfLight = light->pdf(lightSam) * scene->getLightPdf();
        return pdfBsdf * bsdfSpec * lightSpec / (pdfBsdf + pdfLight);
    }
}