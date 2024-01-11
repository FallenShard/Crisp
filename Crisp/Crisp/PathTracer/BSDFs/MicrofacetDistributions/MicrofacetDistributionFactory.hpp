#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/MicrofacetDistribution.hpp>

namespace crisp {
class MicrofacetDistributionFactory {
public:
    static std::unique_ptr<MicrofacetDistribution> create(std::string type, VariantMap parameters);
};
} // namespace crisp