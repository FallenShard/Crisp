#include "SamplerFactory.hpp"

#include "Sampler.hpp"
#include "Independent.hpp"

namespace vesper
{
    std::unique_ptr<Sampler> SamplerFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "independent")
        {
            return std::make_unique<IndependentSampler>(parameters);
        }
        else
        {
            std::cerr << "Unknown sampler type requested! Creating default independentSampler" << std::endl;
            return std::make_unique<IndependentSampler>(parameters);
        }
    }
}