#pragma once

#include "Medium.hpp"

namespace crisp
{
    class HomogeneousMedium : public Medium
    {
    public:
        enum class SamplingStrategy
        {
            Balance,
            Single,
            Manual
        };

        HomogeneousMedium(const VariantMap& params);
        virtual ~HomogeneousMedium();

        virtual void eval(const Ray3& ray, Sample& sample) const override;
        virtual Spectrum evalTransmittance(const Ray3& ray, Sampler& sampler) const override;
        virtual bool sampleDistance(const Ray3& ray, Sample& sample, Sampler& sampler) const override;

        virtual bool isHomogeneous() const override;

    private:
        void eval(float distance, Sample& sample) const;

        float m_samplingDensity;
        float m_mediumSamplingWeight;
        float m_albedo;

        SamplingStrategy m_strategy;
    };

}