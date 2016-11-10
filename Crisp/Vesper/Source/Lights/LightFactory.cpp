#include "LightFactory.hpp"

#include "PointLight.hpp"
#include "DirectionalLight.hpp"

namespace vesper
{
    std::unique_ptr<Light> LightFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "point")
        {
            return std::make_unique<PointLight>(parameters);
        }
        else if (type == "directional")
        {
            return std::make_unique<DirectionalLight>(parameters);
        }
        else
        {
            std::cerr << "Unknown light type \"" << type << "\" requested! Creating default point light" << std::endl;
            return std::make_unique<PointLight>(parameters);
        }
    }
}