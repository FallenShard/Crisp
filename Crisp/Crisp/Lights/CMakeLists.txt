add_cpp_header_library(CrispLightDescriptor
    "LightDescriptor.hpp"
)
target_link_libraries(CrispLightDescriptor
    INTERFACE Crisp::Math
)

add_cpp_static_library(CrispDirectionalLight
    "DirectionalLight.cpp"
    "DirectionalLight.hpp"
)
target_link_libraries(CrispDirectionalLight
    PUBLIC Crisp::LightDescriptor
    PRIVATE Crisp::Checks
)

add_cpp_static_library(CrispCascadedShadowMapping
    "CascadedShadowMapping.cpp"
    "CascadedShadowMapping.hpp"
)
target_link_libraries(CrispCascadedShadowMapping
    PUBLIC Crisp::VulkanRingBuffer
    PUBLIC Crisp::DirectionalLight
    PUBLIC Crisp::Camera
    PRIVATE Crisp::Checks
)

add_cpp_test(
    CrispLightsTest
    "Test/CascadedShadowMappingTest.cpp"
    "Test/DirectionalLightTest.cpp"
)
target_link_libraries(
    CrispLightsTest
    PRIVATE Crisp::CascadedShadowMapping
    PRIVATE Crisp::DirectionalLight
)

add_cpp_static_library(CrispLights
    "EnvironmentLight.cpp"
    "EnvironmentLight.hpp"
    "EnvironmentLightIo.cpp"
    "EnvironmentLightIo.hpp"
    "LightClustering.cpp"
    "LightClustering.hpp"
    "LightSystem.cpp"
    "LightSystem.hpp"
    "PointLight.cpp"
    "PointLight.hpp"

    # "ShadowMapper.cpp"
    # "ShadowMapper.hpp"
)
target_link_libraries(CrispLights
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Geometry
    PUBLIC Crisp::CascadedShadowMapping
    PUBLIC Crisp::DirectionalLight
    PRIVATE Crisp::Logger
    PRIVATE Crisp::DirectionalLight
)
