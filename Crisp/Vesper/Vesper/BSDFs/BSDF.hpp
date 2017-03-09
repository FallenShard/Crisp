#pragma once

#include <glm/glm.hpp>

#include "Spectrums/Spectrum.hpp"
#include "Core/VariantMap.hpp"
#include "Textures/Texture.hpp"

namespace vesper
{
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

        enum Type
        {
            Passthrough,
            Diffuse,
            Glossy,
            Delta
        };

        struct Sample
        {
            glm::vec3 p;
            glm::vec3 wi;
            glm::vec3 wo;

            float eta;
            glm::vec2 uv;

            Measure measure;
            unsigned int requestedType;
            unsigned int sampledType;

            Sample() {}
            Sample(const glm::vec3& p, const glm::vec3& wi) : p(p), wi(wi) {}
            Sample(const glm::vec3& p, const glm::vec3& wi, const glm::vec2& uv) : p(p), wi(wi), uv(uv) {}
            Sample(const glm::vec3& p, const glm::vec3& wi, const glm::vec3& wo) : p(p), wi(wi), wo(wo) {}
            Sample(const glm::vec3& p, const glm::vec3& wi, const glm::vec3& wo, const glm::vec2& uv) : p(p), wi(wi), wo(wo), uv(uv) {}
        };

        virtual ~BSDF() {};
        virtual void setTexture(std::shared_ptr<Texture<float>> texture) {};
        virtual void setTexture(std::shared_ptr<Texture<Spectrum>> texture) {};
        virtual Spectrum eval(const BSDF::Sample& bsdfSample) const = 0;
        virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const = 0;
        virtual float pdf(const BSDF::Sample& bsdfSample) const = 0;
        virtual unsigned int getType() const = 0;
    };
}