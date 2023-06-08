#include "MisPathTracer.hpp"

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/BSSRDFs/BSSRDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp
{
namespace
{
inline float miWeight(float pdf1, float pdf2)
{
    pdf1 *= pdf1;
    pdf2 *= pdf2;
    return pdf1 / (pdf1 + pdf2);
}

Spectrum estimateDirect(
    const pt::Scene& scene,
    Sampler& sampler,
    const Ray3& ray,
    const Intersection& its,
    const Light& light,
    bool specular)
{
    LobeFlags lobe = specular ? LobeFlags(Lobe::Delta | Lobe::Smooth) : LobeFlags(Lobe::Smooth);
    Spectrum Ld(0.0f);

    Light::Sample lightSample(its.p);
    Spectrum Li = light.sample(lightSample, sampler);
    if (lightSample.pdf > 0.0f && !Li.isZero() && !scene.rayIntersect(lightSample.shadowRay))
    {
        BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
        bsdfSample.measure = BSDF::Measure::SolidAngle;
        bsdfSample.eta = 1.0f;
        Spectrum f = its.shape->getBSDF()->eval(bsdfSample);
        float bsdfPdf = its.shape->getBSDF()->pdf(bsdfSample);
        if (!f.isZero() && bsdfPdf > 0.0f)
        {
            float weight = light.isDelta() ? 1.0f : miWeight(lightSample.pdf, bsdfPdf);
            Ld += f * Li * weight;
        }
    }

    if (!light.isDelta())
    {
        BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
        Spectrum f = its.shape->getBSDF()->sample(bsdfSample, sampler);
        bool sampledSpecular = bsdfSample.sampledLobe == Lobe::Delta;

        if (!f.isZero() && bsdfSample.pdf > 0.0f)
        {
            float weight = 1.0f;
            if (!sampledSpecular)
            {
                Intersection bsdfIts;
                Ray3 bsdfRay(its.p, its.toWorld(bsdfSample.wo));
                if (!scene.rayIntersect(bsdfRay, bsdfIts))
                {
                    if (scene.getEnvironmentLight())
                    {
                        Light::Sample envLightSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
                        envLightSample.wi = bsdfRay.d;
                        float lightPdf = scene.getEnvironmentLight()->pdf(envLightSample);
                        weight = miWeight(bsdfSample.pdf, lightPdf);
                    }
                    else
                        return Ld;
                }
                else if (bsdfIts.shape->getLight() == &light)
                {
                    Light::Sample lSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
                    lSample.wi = bsdfRay.d;
                    float lightPdf = light.pdf(lSample);
                    weight = miWeight(bsdfSample.pdf, lightPdf);
                }
                else
                    return Ld;
            }
            Intersection bsdfIts;
            Ray3 bsdfRay(its.p, its.toWorld(bsdfSample.wo));
            bool foundIntersection = scene.rayIntersect(bsdfRay, bsdfIts);

            Spectrum bLi(0.0f);
            if (foundIntersection)
            {
                if (bsdfIts.shape->getLight() == &light)
                {
                    Light::Sample alightSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
                    alightSample.wi = bsdfRay.d;
                    bLi = light.eval(alightSample);
                }
            }
            else if (scene.getEnvironmentLight())
            {
                Light::Sample alightSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
                alightSample.wi = bsdfRay.d;
                bLi = scene.getEnvironmentLight()->eval(alightSample);
            }

            if (!Li.isZero())
                Ld += f * bLi * weight;
        }
    }
    return Ld;
}

Spectrum uniformSampleOneLight(const pt::Scene& scene, Sampler& sampler, const Ray3& ray, const Intersection& its)
{
    Light::Sample lightSample(its.p);
    auto light = scene.getRandomLight(sampler.next1D());
    if (!light)
        return Spectrum(0.0f);

    float pickPdf = scene.getLightPdf();

    return estimateDirect(scene, sampler, ray, its, *light, false) / pickPdf;
}
} // namespace

MisPathTracerIntegrator::MisPathTracerIntegrator(const VariantMap& attribs)
{
    m_rrDepth = static_cast<unsigned int>(attribs.get<int>("rrDepth", 5));
    m_maxDepth = static_cast<unsigned int>(attribs.get<int>("maxDepth", std::numeric_limits<int>::max()));
}

MisPathTracerIntegrator::~MisPathTracerIntegrator() {}

void MisPathTracerIntegrator::preprocess(pt::Scene* scene)
{
    for (auto& shape : scene->getShapes())
        if (shape->getBSSRDF())
            shape->getBSSRDF()->preprocess(shape.get(), scene);
}

Spectrum MisPathTracerIntegrator::Li(
    const pt::Scene* scene, Sampler& sampler, Ray3& r, IlluminationFlags /*illumFlags*/) const
{
    Spectrum L(0.0f);
    Spectrum throughput(1.0f);
    Ray3 ray(r);
    unsigned int bounces = 0;
    bool specularBounce = false;

    Intersection its;

    while (true)
    {
        bool foundIntersection = scene->rayIntersect(ray, its);

        if (bounces == 0 || specularBounce)
        {
            if (foundIntersection)
            {
                if (auto light = its.shape->getLight())
                {
                    Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
                    L += throughput * light->eval(lightSample);
                }
            }
            else
            {
                L += throughput * scene->evalEnvLight(ray);
            }
        }

        if (!foundIntersection || bounces >= m_maxDepth)
            break;

        if (!(its.shape->getBSDF()->getLobeType() & Lobe::Delta))
        {
            Spectrum lightMis = uniformSampleOneLight(*scene, sampler, ray, its);
            // L += path.throughput * lightImportanceSample(scene, sampler, ray, its);
            L += throughput * lightMis;
        }

        BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
        Spectrum f = its.shape->getBSDF()->sample(bsdfSample, sampler);
        if (f.isZero() || bsdfSample.pdf == 0.0f)
            break;

        throughput *= f;

        specularBounce = bsdfSample.sampledLobe == Lobe::Delta;

        ray = Ray3(its.p, its.toWorld(bsdfSample.wo));

        if (bounces > m_rrDepth)
        {
            float q = 1.0f - std::min(throughput.maxCoeff(), 0.99f);
            if (sampler.next1D() < q)
                break;

            throughput /= (1.0f - q);
        }

        bounces++;
    }

    return L;
}

Spectrum MisPathTracerIntegrator::lightImportanceSample(
    const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const
{
    Light::Sample lightSample(its.p);
    Spectrum Li = scene->sampleLight(its, sampler, lightSample);
    if (lightSample.pdf <= 0.0f || Li.isZero() || scene->rayIntersect(lightSample.shadowRay))
        return Spectrum(0.0f);

    BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
    bsdfSample.measure = BSDF::Measure::SolidAngle;
    bsdfSample.eta = 1.0f;
    Spectrum f = its.shape->getBSDF()->eval(bsdfSample);
    if (f.isZero())
        return Spectrum(0.0f);

    float pdfLight = lightSample.pdf;
    float pdfBsdf = lightSample.light->isDelta() ? 0.0f : its.shape->getBSDF()->pdf(bsdfSample);
    return f * Li * miWeight(pdfLight, pdfBsdf);
}

Spectrum MisPathTracerIntegrator::bsdfImportanceSample(
    const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its, Path& path) const
{
    BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
    Spectrum f = its.shape->getBSDF()->sample(bsdfSample, sampler);
    if (f.isZero())
        return Spectrum(0.0f);

    Intersection bsdfIts;
    Ray3 bsdfRay(its.p, its.toWorld(bsdfSample.wo));

    path.isSpecular = bsdfSample.sampledLobe == Lobe::Delta;
    path.bsdfSample = f;
    path.nextRay = bsdfRay;

    const Light* hitLight = nullptr;

    if (!scene->rayIntersect(bsdfRay, bsdfIts))
        hitLight = nullptr; // scene->getEnvironmentLight();
    else
        hitLight = bsdfIts.shape->getLight();

    if (!hitLight)
        return Spectrum(0.0f);

    Light::Sample lightSample(its.p, bsdfIts.p, bsdfIts.shFrame.n);
    lightSample.wi = bsdfRay.d;
    Spectrum Li = hitLight->eval(lightSample);

    float pdfBsdf = bsdfSample.pdf;
    float pdfLight = path.isSpecular ? 0.0f : hitLight->pdf(lightSample) * scene->getLightPdf();
    return f * Li * miWeight(pdfBsdf, pdfLight);
}
} // namespace crisp