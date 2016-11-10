#pragma once

#include "Spectrums/Spectrum.hpp"
#include "Math/Ray.hpp"
#include "Core/VariantMap.hpp"

namespace vesper
{
    class Scene;
    class Sampler;

    class Integrator
    {
    public:
        virtual ~Integrator();

        virtual void preprocess(const Scene* scene) = 0;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const = 0;
    };
}