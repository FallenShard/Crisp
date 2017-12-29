#pragma once

#include "Integrator.hpp"

namespace vesper
{
    struct Intersection;

    class MisDirectLightingIntegrator : public Integrator
    {
    public:
        MisDirectLightingIntegrator(const VariantMap& params = VariantMap());
        virtual ~MisDirectLightingIntegrator();

        virtual void preprocess(Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;

    private:
        Spectrum lightImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const;
        Spectrum bsdfImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const;
    };
}