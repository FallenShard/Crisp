#pragma once

#include "Integrator.hpp"

namespace vesper
{
    struct Intersection;

    class VolumePathTracerIntegrator : public Integrator
    {
    public:
        VolumePathTracerIntegrator(const VariantMap& attributes);
        virtual ~VolumePathTracerIntegrator();

        virtual void preprocess(const Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const override;

    private:
        unsigned int m_rrDepth;
        unsigned int m_maxDepth;
    };
}