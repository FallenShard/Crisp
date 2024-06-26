#pragma once

#include <Crisp/PathTracer/Integrators/Integrator.hpp>

namespace crisp {
struct Intersection;

class MisPathTracerIntegrator : public Integrator {
public:
    MisPathTracerIntegrator(const VariantMap& attributes);
    virtual ~MisPathTracerIntegrator();

    virtual void preprocess(pt::Scene* scene) override;
    virtual Spectrum Li(
        const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;

private:
    struct Path {
        Ray3 currRay;
        Ray3 nextRay;
        Spectrum throughput;
        Spectrum bsdfSample;
        bool isSpecular;
        unsigned int bounces;

        Path(const Ray3& startRay)
            : currRay(startRay)
            , nextRay()
            , throughput(1.0f)
            , bsdfSample(1.0f)
            , isSpecular(false)
            , bounces(0) {}
    };

    Spectrum bsdfImportanceSample(
        const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its, Path& path) const;
    Spectrum lightImportanceSample(
        const pt::Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const;

    unsigned int m_rrDepth;
    unsigned int m_maxDepth;
};
} // namespace crisp