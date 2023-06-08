#pragma once

#include <memory>
#include <string>
#include <functional>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "MicrofacetDistribution.hpp"

namespace crisp
{
    class MicrofacetDistributionFactory
    {
    public:
        static std::unique_ptr<MicrofacetDistribution> create(std::string type, VariantMap parameters);
    };
}