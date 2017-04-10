#pragma once

#include "MicrofacetDistribution.hpp"

namespace vesper
{
    class GGXDistribution : public MicrofacetDistribution
    {
    public:
        GGXDistribution(const VariantMap& params);
        ~GGXDistribution();

        virtual glm::vec3 sampleNormal(const glm::vec2& sample) const override;
        virtual float pdf(const glm::vec3& m) const override;

        virtual float D(const glm::vec3& m) const override;
        virtual float G(const glm::vec3& wi, const glm::vec3& wo, const glm::vec3& m) const override;

    private:
        float smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const;
    };
}