#include "MediumFactory.hpp"

#include "Homogeneous.hpp"

namespace crisp
{
    std::unique_ptr<Medium> MediumFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "isotropic")
        {
            return std::make_unique<HomogeneousMedium>(parameters);
        }
        else
        {
            std::cerr << "Unknown medium type \"" << type << "\" requested! Creating default homogeneous type" << std::endl;
            return std::make_unique<HomogeneousMedium>(parameters);
        }
    }
}