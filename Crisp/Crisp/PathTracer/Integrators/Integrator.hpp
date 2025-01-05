#pragma once

#include <Crisp/Math/Ray.hpp>
#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Spectra/Spectrum.hpp>
#include <Crisp/Utils/BitFlags.hpp>

namespace crisp {
namespace pt {
class Scene;
} // namespace pt
class Sampler;

enum class Illumination { Direct = 1 << 0, Indirect = 1 << 1, Full = Direct | Indirect };
DECLARE_BITFLAG(Illumination);

class Integrator {
public:
    virtual ~Integrator() = default;

    virtual void preprocess(pt::Scene* scene) = 0;
    virtual Spectrum Li(
        const pt::Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const = 0;
};
} // namespace crisp