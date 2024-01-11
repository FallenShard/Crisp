#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/MicrofacetDistributionFactory.hpp>

#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/Beckmann.hpp>
#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/GGX.hpp>
#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/Phong.hpp>

namespace crisp {
std::unique_ptr<MicrofacetDistribution> MicrofacetDistributionFactory::create(std::string type, VariantMap params) {
    if (type == "beckmann") {
        return std::make_unique<BeckmannDistribution>(params);
    } else if (type == "ggx") {
        return std::make_unique<GGXDistribution>(params);
    } else if (type == "phong") {
        return std::make_unique<PhongDistribution>(params);
    } else {
        std::cout << "Unknown microfacet distribution specified! Creating Beckmann as default!\n";
        return std::make_unique<BeckmannDistribution>(params);
    }
}
} // namespace crisp
