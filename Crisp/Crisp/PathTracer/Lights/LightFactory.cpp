#include <Crisp/PathTracer/Lights/LightFactory.hpp>

#include <Crisp/PathTracer/Lights/AreaLight.hpp>
#include <Crisp/PathTracer/Lights/DirectionalLight.hpp>
#include <Crisp/PathTracer/Lights/EnvironmentLight.hpp>
#include <Crisp/PathTracer/Lights/PointLight.hpp>

namespace crisp {
std::unique_ptr<Light> LightFactory::create(std::string type, VariantMap parameters) {
    if (type == "point") {
        return std::make_unique<PointLight>(parameters);
    } else if (type == "directional") {
        return std::make_unique<DirectionalLight>(parameters);
    } else if (type == "area") {
        return std::make_unique<AreaLight>(parameters);
    } else if (type == "environment") {
        return std::make_unique<EnvironmentLight>(parameters);
    } else {
        std::cerr << "Unknown light type \"" << type << "\" requested! Creating default point light" << std::endl;
        return std::make_unique<PointLight>(parameters);
    }
}
} // namespace crisp