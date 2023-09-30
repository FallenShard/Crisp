#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Textures/Texture.hpp>
#include <Crisp/Spectra/Spectrum.hpp>
#include <Crisp/Utils/BitFlags.hpp>

namespace crisp {
enum class Lobe {
    Passthrough = 1 << 0,
    Diffuse = 1 << 1,
    Glossy = 1 << 2,
    Delta = 1 << 3,

    Smooth = Diffuse | Glossy
};

DECLARE_BITFLAG(Lobe)

class BSDF {
public:
    enum class Measure {
        Unknown,
        SolidAngle,
        Discrete
    };

    struct Sample {
        glm::vec3 p;
        glm::vec2 uv;
        glm::vec3 wi;

        glm::vec3 wo;
        float pdf;

        Measure measure;
        Lobe sampledLobe;

        float eta;

        Sample()
            : p(0.0f)
            , uv(0.0f)
            , wi(0.0f)
            , wo(0.0f)
            , pdf(0.0f)
            , measure(Measure::Unknown)
            , sampledLobe(Lobe::Passthrough)
            , eta(0.0f) {}

        Sample(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wi)
            : p(p)
            , wi(wi)
            , uv(uv)
            , wo(0.0f)
            , pdf(0.0f)
            , measure(Measure::Unknown)
            , sampledLobe(Lobe::Passthrough)
            , eta(0.0f) {}

        Sample(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wi, const glm::vec3& wo)
            : p(p)
            , uv(uv)
            , wi(wi)
            , wo(wo)
            , pdf(0.0f)
            , measure(Measure::Unknown)
            , sampledLobe(Lobe::Passthrough)
            , eta(0.0f) {}
    };

    BSDF(LobeFlags lobeFlags);
    virtual ~BSDF();

    virtual void setTexture(std::shared_ptr<Texture<float>> texture);
    virtual void setTexture(std::shared_ptr<Texture<Spectrum>> texture);

    // Computes f(x, wi, wo) * cosTheta(wo)
    virtual Spectrum eval(const BSDF::Sample& bsdfSample) const = 0;

    // Samples a direction wo and computes f(x, wi, wo) * cosTheta(wo) / pdf(wo)
    virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const = 0;
    virtual float pdf(const BSDF::Sample& bsdfSample) const = 0;

    LobeFlags getLobeType() const;

protected:
    LobeFlags m_lobe;
};
} // namespace crisp