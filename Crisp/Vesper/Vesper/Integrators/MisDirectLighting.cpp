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
    namespace
    {
        inline float powerHeuristic(float fPdf, float gPdf)
        {
            float fPdf2 = fPdf * fPdf;
            float gPdf2 = gPdf * gPdf;
            return fPdf2 / (fPdf2 + gPdf2);
        }

        inline float balanceHeuristic(float fPdf, float gPdf)
        {
            return fPdf / (fPdf + gPdf);
        }
    }

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
            return scene->evalEnvLight(ray);

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
        Spectrum Li = scene->sampleLight(its, sampler, lightSample);
        if (lightSample.pdf <= 0.0f || Li.isZero() || scene->rayIntersect(lightSample.shadowRay))
            return Spectrum(0.0f);

        BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
        bsdfSample.measure = BSDF::Measure::SolidAngle;
        bsdfSample.eta     = 1.0f;
        auto f = its.shape->getBSDF()->eval(bsdfSample);
        if (f.isZero())
            return Spectrum(0.0f);

        float pdfLight = lightSample.pdf;
        float pdfBsdf = lightSample.light->isDelta() ? 0.0f : its.shape->getBSDF()->pdf(bsdfSample);
        return f * Li * powerHeuristic(pdfLight, pdfBsdf);
    }

    Spectrum MisDirectLightingIntegrator::bsdfImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
    {
        BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
        auto f = its.shape->getBSDF()->sample(bsdfSample, sampler);
        if (f.isZero())
            return Spectrum(0.0f);
        
        Intersection bsdfIts;
        Ray3 bsdfRay(its.p, its.toWorld(bsdfSample.wo));
        const Light* hitLight = nullptr;

        if (!scene->rayIntersect(bsdfRay, bsdfIts))
        {
            hitLight = scene->getEnvironmentLight();
        }
        else if (bsdfIts.shape->getLight())
        {
            hitLight = bsdfIts.shape->getLight();
        }

        if (!hitLight)
            return Spectrum(0.0f);

        Light::Sample lightSam(its.p, bsdfIts.p, bsdfIts.shFrame.n);
        lightSam.wi = bsdfRay.d;
        auto Li = hitLight->eval(lightSam);
        if (Li.isZero())
            return Spectrum(0.0f);

        float pdfBsdf = bsdfSample.pdf;
        float pdfLight = bsdfSample.sampledLobe == Lobe::Delta ? 0.0f : hitLight->pdf(lightSam) * scene->getLightPdf();
        return f * Li * powerHeuristic(pdfBsdf, pdfLight);
    }
}