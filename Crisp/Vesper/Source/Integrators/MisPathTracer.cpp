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

        Spectrum throughput(1.f);
        bool isSpecular = true;

        while (true)
        {
            if (!scene->rayIntersect(ray, its))
                break;

            if (isSpecular && its.shape->getLight())
            {
                Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
                L += throughput * its.shape->getLight()->eval(lightSample);
            }

            if (its.shape->getBSDF()->getType() != BSDF::Type::Delta)
            {
                Light::Sample lightSample(its.p);
                auto lightSpec = scene->sampleLight(its, sampler, lightSample);

                float cosFactor = glm::dot(its.shFrame.n, lightSample.wi);
                if (!(cosFactor <= 0.0f || lightSpec.isZero() || scene->rayIntersect(lightSample.shadowRay)))
                {
                    BSDF::Sample bsdfSam(its.p, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
                    bsdfSam.measure = BSDF::Measure::SolidAngle;
                    auto bsdfSpec = its.shape->getBSDF()->eval(bsdfSam);

                    float pdfEm = lightSample.pdf;
                    float pdfBsdf = its.shape->getBSDF()->pdf(bsdfSam);
                    L += throughput * bsdfSpec * lightSpec * cosFactor * miWeight(pdfEm, pdfBsdf);
                }
            }

            BSDF::Sample bsdfSample(its.p, its.toLocal(-ray.d));
            auto bsdf = its.shape->getBSDF()->sample(bsdfSample, sampler);
            Intersection bsdfIts;
            Ray3 bsdfRay(its.p, its.toWorld(bsdfSample.wo));
            if (scene->rayIntersect(bsdfRay, bsdfIts) && bsdfIts.shape->getLight())
            {
                auto light = bsdfIts.shape->getLight();

                Light::Sample lightSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
                lightSample.wi = bsdfRay.d;
                auto lightSpec = light->eval(lightSample);

                float pdfBsdf = its.shape->getBSDF()->pdf(bsdfSample);
                float pdfEm = light->pdf(lightSample) * scene->getLightPdf();
                if (pdfBsdf + pdfEm > 0.0f)
                    L += throughput * bsdf * lightSpec * miWeight(pdfBsdf, pdfEm);
            }

            isSpecular = its.shape->getBSDF()->getType() == BSDF::Type::Delta;
            throughput *= bsdf;
            ray = bsdfRay;

            float q = 1.f - std::min(throughput.maxCoeff(), 0.99f);
            if (sampler.next1D() > q)
                throughput /= (1.f - q);
            else
                break;
        }

        return L;
    }
}