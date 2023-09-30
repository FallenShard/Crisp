#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/Spectra/Spectrum.hpp>

namespace crisp {
class MicrofacetDistribution {
public:
    virtual ~MicrofacetDistribution(){};

    virtual glm::vec3 sampleNormal(const glm::vec2& sample) const = 0;
    virtual float pdf(const glm::vec3& m) const = 0;

    virtual float D(const glm::vec3& m) const = 0;
    virtual float G(const glm::vec3& wi, const glm::vec3& wo, const glm::vec3& m) const = 0;

protected:
    float m_alpha;
};
} // namespace crisp