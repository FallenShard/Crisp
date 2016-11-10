#include "BSDFFactory.hpp"

#include "BSDF.hpp"
#include "Lambertian.hpp"
#include "Dielectric.hpp"

namespace vesper
{
    std::unique_ptr<BSDF> BSDFFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "lambertian")
        {
            return std::make_unique<LambertianBSDF>(parameters);
        }
        else if (type == "dielectric")
        {
            return std::make_unique<DielectricBSDF>(parameters);
        }
        else
        {
            std::cerr << "Unknown bsdf type \"" << type << "\" requested! Creating default lambertian bsdf" << std::endl;
            return std::make_unique<LambertianBSDF>(parameters);
        }
    }
}