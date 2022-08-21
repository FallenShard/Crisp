add_cpp_static_library(CrispLights
    "DirectionalLight.cpp"
    "DirectionalLight.hpp"
    "EnvironmentLighting.cpp"
    "EnvironmentLighting.hpp"
    "ForwardClusteredShading.cpp"
    "ForwardClusteredShading.hpp"
    "LightDescriptor.hpp"
    "LightSystem.cpp"
    "LightSystem.hpp"
    "PointLight.cpp"
    "PointLight.hpp"
    "ShadowMapper.cpp"
    "ShadowMapper.hpp")
target_link_libraries(CrispLights
    PUBLIC CrispRenderer
    PUBLIC CrispGeometry
    PUBLIC CrispUtils
    PUBLIC spdlog
    PUBLIC robin_hood)