#pragma once

#include <random>

#include "Sampler.hpp"

namespace vesper
{
    class IndependentSampler : public Sampler
    {
    public:
        IndependentSampler(const VariantMap& attribs = VariantMap());
        ~IndependentSampler();

        virtual std::unique_ptr<Sampler> clone() const override;

        virtual void prepare() override;
        virtual void generate() override;
        virtual void advance() override;

        virtual float next1D() override;
        virtual glm::vec2 next2D() override;

    private:
        inline float nextFloat();

        std::default_random_engine m_randomEngine;
        std::uniform_real_distribution<float> m_distribution;
    };
}