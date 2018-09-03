#pragma once

#include <glm/glm.hpp>

#include <CrispCore/BitFlags.hpp>
#include "Spectrums/Spectrum.hpp"
#include "Core/VariantMap.hpp"
#include "Textures/Texture.hpp"

namespace crisp
{
    enum class Lobe
    {
        Passthrough = 1 << 0,
        Diffuse = 1 << 1,
        Glossy = 1 << 2,
        Delta = 1 << 3,

        Smooth = Diffuse | Glossy
    };

    DECLARE_BITFLAG(Lobe)

    class Sampler;

    class BSDF
    {
    public:
        enum class Measure
        {
            Unknown,
            SolidAngle,
            Discrete
        };

        struct Sample
        {
            glm::vec3 p;
            glm::vec2 uv;
            glm::vec3 wi;

            glm::vec3 wo;
            float pdf;

            Measure measure;
            Lobe    sampledLobe;

            float eta;

            Sample() {}
            Sample(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wi) : p(p), wi(wi), uv(uv) {}
            Sample(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wi, const glm::vec3& wo) : p(p), uv(uv), wi(wi), wo(wo) {}
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
}