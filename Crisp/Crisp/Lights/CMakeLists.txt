add_cpp_static_library(CrispLights
    "CascadedShadowMapping.cpp"
    "CascadedShadowMapping.hpp"
    "DirectionalLight.cpp"
    "DirectionalLight.hpp"
    "EnvironmentLighting.cpp"
    "EnvironmentLighting.hpp"
    "LightClustering.cpp"
    "LightClustering.hpp"
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