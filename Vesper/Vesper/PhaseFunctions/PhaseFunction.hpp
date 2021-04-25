#pragma once

#include "Core/VariantMap.hpp"
#include "Media/Medium.hpp"

namespace crisp
{
    class Sampler;

    class PhaseFunction
    {
    public:
        struct Sample
        {
            const Medium::Sample& mediumSample;

            glm::vec3 wi;
            glm::vec3 wo;

            float pdf;

            float eta;

            Sample(const Medium::Sample& mSam, const glm::vec3& wi) : mediumSample(mSam), wi(wi) {}
            Sample(const Medium::Sample& mSam, const glm::vec3& wi, const glm::vec3& wo) : mediumSample(mSam), wi(wi), wo(wo) {}
        };

        virtual ~PhaseFunction() {}

        virtual float eval(const Sample& pfSample) const = 0;
        virtual float sample(Sample& pfSample, Sampler& sampler) const = 0;
        virtual float pdf(const Sample& pfSample) const = 0;
        
    };
}