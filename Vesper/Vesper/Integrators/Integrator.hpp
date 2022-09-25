#pragma once

#include <Crisp/Math/Ray.hpp>
#include <Crisp/Utils/BitFlags.hpp>


#include "Core/VariantMap.hpp"
#include <Crisp/Spectra/Spectrum.hpp>

namespace crisp
{
class Scene;
class Sampler;

enum class Illumination
{
    Direct = 1 << 0,
    Indirect = 1 << 1,
    Full = Direct | Indirect
};
DECLARE_BITFLAG(Illumination);

class Integrator
{
public:
    virtual ~Integrator();

    virtual void preprocess(Scene* scene) = 0;
    virtual Spectrum Li(
        const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const = 0;
};
} // namespace crisp