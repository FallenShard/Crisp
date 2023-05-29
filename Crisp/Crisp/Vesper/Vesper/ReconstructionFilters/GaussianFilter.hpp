#pragma once

#include "ReconstructionFilter.hpp"

namespace crisp
{
    class GaussianFilter : public ReconstructionFilter
    {
    public:
        GaussianFilter();
        virtual float eval(float x, float y) const override;

    private:
        float m_sigma;
    };
}