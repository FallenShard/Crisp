#include "MisPathTracer.hpp"

#include "Samplers/Sampler.hpp"
#include "Core/Scene.hpp"
#include "BSDFs/BSDF.hpp"
#include "Core/Intersection.hpp"
#include "Shapes/Shape.hpp"

namespace vesper
{
    inline float miWeight(float pdf1, float pdf2)
    {
        pdf1 *= pdf1;
        pdf2 *= pdf2;
        return pdf1 / (pdf1 + pdf2);
    }

    MisPathTracerIntegrator::MisPathTracerIntegrator(const VariantMap& attribs)
    {
        m_rrDepth = static_cast<unsigned int>(attribs.get<int>("rrDepth", 5));
        m_maxDepth = static_cast<unsigned int>(attribs.get<int>("maxDepth", std::numeric_limits<int>::max()));
    }

    MisPathTracerIntegrator::~MisPathTracerIntegrator()
    {
    }

    void MisPathTracerIntegrator::preprocess(const Scene* scene)
    {
    }

    Spectrum MisPathTracerIntegrator::Li(const Scene* scene, Sampler& sampler, Ray3& ray) const
    {
        Spectrum L(0.f);

        Intersection its;

        Spectrum throughput(1.0f);
        bool isDeltaBounce = true;
        unsigned int bounces = 0;

        while (true)
        {
            // When there's no intersection, evaluate environment light
            bool foundIntersection = scene->rayIntersect(ray, its);

            // If the previous bounce is specular and we hit a light source,
            // evaluate it because we couldn't otherwise evaluate light from a delta bounce
            if (isDeltaBounce)
            {
                if (foundIntersection)
                {
                    if (its.shape->getLight())
                    {
                        Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
                        L += throughput * its.shape->getLight()->eval(lightSample);
                    }
                }
                else // check for environment light
                {
                    L += throughput * scene->evalEnvLight(ray);
                }
            }

            if (!foundIntersection || bounces >= m_maxDepth)
                break;

            // If we did not hit a delta BRDF (mirror, glass etc.), sample the light
            if (!(its.shape->getBSDF()->getLobeType() & Lobe::Delta))
            {
                L += throughput * lightImportanceSample(scene, sampler, ray, its);
            }

            // Sample the BSDF to continue bouncing
            BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
            auto bsdf = its.shape->getBSDF()->sample(bsdfSample, sampler);
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

            if (hitLight)
            {
                Light::Sample lightSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
                lightSample.wi = bsdfRay.d;
                auto lightSpec = hitLight->eval(lightSample);

                float pdfBsdf = bsdfSample.pdf;
                float pdfEm = bsdfSample.sampledLobe == Lobe::Delta ? 0.0f : hitLight->pdf(lightSample) * scene->getLightPdf();
                if (pdfBsdf + pdfEm > 0.0f)
                    L += throughput * bsdf * lightSpec * miWeight(pdfBsdf, pdfEm);
            }

            isDeltaBounce = bsdfSample.sampledLobe == Lobe::Delta;
            throughput *= bsdf;
            ray = bsdfRay;

            bounces++;
            if (bounces > m_rrDepth)
            {
                float q = 1.0f - std::min(throughput.maxCoeff(), 0.99f);
                if (sampler.next1D() > q)
                    throughput /= (1.0f - q);
                else
                    break;
            }
        }

        return L;
    }

    Spectrum MisPathTracerIntegrator::lightImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
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
        return f * Li * miWeight(pdfLight, pdfBsdf);
    }

    Spectrum MisPathTracerIntegrator::bsdfImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
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

        Light::Sample lightSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
        lightSample.wi = bsdfRay.d;
        auto Li = hitLight->eval(lightSample);

        float bsdfPdf = bsdfSample.pdf;
        float lightPdf = bsdfSample.sampledLobe == Lobe::Delta ? 0.0f : hitLight->pdf(lightSample) * scene->getLightPdf();
        return f * Li * miWeight(bsdfPdf, lightPdf);
    }
}