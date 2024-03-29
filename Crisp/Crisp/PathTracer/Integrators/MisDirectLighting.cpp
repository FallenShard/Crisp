#include <Crisp/PathTracer/Integrators/MisDirectLighting.hpp>

#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/BSSRDFs/BSSRDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

namespace crisp {
namespace {
inline float powerHeuristic(float fPdf, float gPdf) {
    float fPdf2 = fPdf * fPdf;
    float gPdf2 = gPdf * gPdf;
    return fPdf2 / (fPdf2 + gPdf2);
}

[[maybe_unused]] inline float balanceHeuristic(float fPdf, float gPdf) {
    return fPdf / (fPdf + gPdf);
}
} // namespace

MisDirectLightingIntegrator::MisDirectLightingIntegrator(const VariantMap& /*params*/) {}

void MisDirectLightingIntegrator::preprocess(pt::Scene* /*scene*/) {}

Spectrum MisDirectLightingIntegrator::Li(
    const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags /*illumFlags*/) const {
    Intersection its;
    if (!scene->rayIntersect(ray, its)) {
        return scene->evalEnvLight(ray);
    }

    Spectrum L(0.0f);

    if (its.shape->getLight()) {
        Light::Sample lightSample(ray.o, its.p, its.shFrame.n);
        L += its.shape->getLight()->eval(lightSample);
    }

    L += lightImportanceSample(scene, sampler, ray, its);
    L += bsdfImportanceSample(scene, sampler, ray, its);

    return L;
}

Spectrum MisDirectLightingIntegrator::lightImportanceSample(
    const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) {
    Light::Sample lightSample(its.p);
    Spectrum Li = scene->sampleLight(its, sampler, lightSample);
    if (lightSample.pdf <= 0.0f || Li.isZero() || scene->rayIntersect(lightSample.shadowRay)) {
        return Spectrum::zero();
    }

    BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d), its.toLocal(lightSample.wi));
    bsdfSample.measure = BSDF::Measure::SolidAngle;
    bsdfSample.eta = 1.0f;
    auto f = its.shape->getBSDF()->eval(bsdfSample);
    if (f.isZero()) {
        return Spectrum::zero();
    }

    const float pdfLight = lightSample.pdf;
    const float pdfBsdf = lightSample.light->isDelta() ? 0.0f : its.shape->getBSDF()->pdf(bsdfSample);
    return f * Li * powerHeuristic(pdfLight, pdfBsdf);
}

Spectrum MisDirectLightingIntegrator::bsdfImportanceSample(
    const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) {
    BSDF::Sample bsdfSample(its.p, its.uv, its.toLocal(-ray.d));
    const auto f = its.shape->getBSDF()->sample(bsdfSample, sampler);
    if (f.isZero()) {
        return Spectrum::zero();
    }

    Intersection bsdfIts;
    Ray3 bsdfRay(its.p, its.toWorld(bsdfSample.wo));
    const Light* hitLight = nullptr;

    if (!scene->rayIntersect(bsdfRay, bsdfIts)) {
        hitLight = scene->getEnvironmentLight();
    } else if (bsdfIts.shape->getLight()) {
        hitLight = bsdfIts.shape->getLight();
    }

    if (!hitLight) {
        return Spectrum::zero();
    }

    Light::Sample lightSam(its.p, bsdfIts.p, bsdfIts.shFrame.n);
    lightSam.wi = bsdfRay.d;
    const auto Li = hitLight->eval(lightSam);
    if (Li.isZero()) {
        return Spectrum::zero();
    }

    const float pdfBsdf = bsdfSample.pdf;
    const float pdfLight =
        bsdfSample.sampledLobe == Lobe::Delta ? 0.0f : hitLight->pdf(lightSam) * scene->getLightPdf();
    return f * Li * powerHeuristic(pdfBsdf, pdfLight);
}
} // namespace crisp