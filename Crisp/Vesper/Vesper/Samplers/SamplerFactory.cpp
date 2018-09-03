#include "SamplerFactory.hpp"

#include "Sampler.hpp"
#include "Independent.hpp"
#include "Fixed.hpp"

namespace crisp
{
    std::unique_ptr<Sampler> SamplerFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "independent")
        {
            return std::make_unique<IndependentSampler>(parameters);
        }
        else if (type == "fixed")
        {
            return std::make_unique<FixedSampler>(parameters);
        }
        else
        {
            std::cerr << "Unknown sampler type requested! Creating default independentSampler" << std::endl;
            return std::make_unique<IndependentSampler>(parameters);
        }
    }
}