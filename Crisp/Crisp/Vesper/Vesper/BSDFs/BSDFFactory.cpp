#include "BSDFFactory.hpp"

#include "BSDF.hpp"
#include "LambertianBSDF.hpp"
#include "DielectricBSDF.hpp"
#include "Mirror.hpp"
#include "Microfacet.hpp"
#include "RoughDielectric.hpp"
#include "SmoothConductor.hpp"
#include "RoughConductor.hpp"

namespace crisp
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
        else if (type == "mirror")
        {
            return std::make_unique<MirrorBSDF>(parameters);
        }
        else if (type == "microfacet")
        {
            return std::make_unique<MicrofacetBSDF>(parameters);
        }
        else if (type == "rough-dielectric")
        {
            return std::make_unique<RoughDielectricBSDF>(parameters);
        }
        else if (type == "smooth-conductor")
        {
            return std::make_unique<SmoothConductorBSDF>(parameters);
        }
        else if (type == "rough-conductor")
        {
            return std::make_unique<RoughConductorBSDF>(parameters);
        }
        else
        {
            std::cerr << "Unknown bsdf type \"" << type << "\" requested! Creating default lambertian bsdf" << std::endl;
            return std::make_unique<LambertianBSDF>(parameters);
        }
    }
}