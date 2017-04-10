#include "MicrofacetDistributionFactory.hpp"

#include "Beckmann.hpp"
#include "GGX.hpp"
#include "Phong.hpp"

namespace vesper
{
    std::unique_ptr<MicrofacetDistribution> MicrofacetDistributionFactory::create(std::string type, VariantMap params)
    {
        if (type == "beckmann")
            return std::make_unique<BeckmannDistribution>(params);
        else if (type == "ggx")
            return std::make_unique<GGXDistribution>(params);
        else if (type == "phong")
            return std::make_unique<PhongDistribution>(params);
        else
        {
            std::cout << "Unknown microfacet distribution specified! Creating Beckmann as default!\n";
            return std::make_unique<BeckmannDistribution>(params);
        }
    }
}

