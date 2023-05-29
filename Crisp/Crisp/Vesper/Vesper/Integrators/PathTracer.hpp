#pragma once

#include "Integrator.hpp"

namespace crisp
{
    class PathTracerIntegrator : public Integrator
    {
    public:
        PathTracerIntegrator(const VariantMap& attributes);
        virtual ~PathTracerIntegrator();

        virtual void preprocess(Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;
    };
}