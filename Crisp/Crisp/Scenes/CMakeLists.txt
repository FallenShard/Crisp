add_cpp_static_library(CrispSceneContainer
    "SceneContainer.cpp"
    "SceneContainer.hpp"
)
target_link_libraries(CrispSceneContainer
    PUBLIC Crisp::Scene
    PUBLIC Crisp::JsonUtils

    PRIVATE Crisp::AtmosphereScene
    PRIVATE Crisp::PbrScene
    PRIVATE Crisp::ShadowMappingScene
    PRIVATE Crisp::MaterialExplorerScene
    PRIVATE Crisp::GltfViewerScene
    PRIVATE Crisp::ClusteredLightingScene
    PRIVATE Crisp::OceanScene
    PRIVATE Crisp::FluidSimulationScene
    PRIVATE Crisp::VulkanRayTracingScene
)

add_cpp_static_library(CrispAtmosphereScene
    "AtmosphereScene.cpp"
    "AtmosphereScene.hpp"
)
target_link_libraries(CrispAtmosphereScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Models
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::AtmosphereGui
    PRIVATE ImGui
)

add_cpp_static_library(CrispScene
    "Scene.cpp"
    "Scene.hpp"
    "TestScene.cpp"
    "TestScene.hpp"
)
target_link_libraries(CrispScene
    PUBLIC Crisp::Window
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::HashMap
)

add_cpp_static_library(CrispPbrScene
    "PbrScene.cpp"
    "PbrScene.hpp"
)

add_cpp_static_library(CrispShadowMappingScene
    "ShadowMappingScene.cpp"
    "ShadowMappingScene.hpp"
)
target_link_libraries(CrispShadowMappingScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::ImGuiCameraUtils
    PRIVATE ImGui
)

add_cpp_static_library(CrispGltfViewerScene
    "GltfViewerScene.cpp"
    "GltfViewerScene.hpp"
)
target_link_libraries(CrispGltfViewerScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::ImGuiCameraUtils
    PRIVATE ImGui
)

add_cpp_static_library(CrispClusteredLightingScene
    "ClusteredLightingScene.cpp"
    "ClusteredLightingScene.hpp"
)
target_link_libraries(CrispClusteredLightingScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::JsonUtils
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::ImGuiCameraUtils
    PRIVATE ImGui
)

add_cpp_static_library(CrispPathTracedView
    "PathTracedView.cpp"
    "PathTracedView.hpp"
)
target_link_libraries(CrispPathTracedView
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Material
    PUBLIC Crisp::VulkanDescriptorHeap
    PUBLIC Crisp::RayTracingPipelineBuilder
    PRIVATE Crisp::RenderGraph
    PRIVATE ImGui
)

add_cpp_static_library(CrispMaterialExplorerScene
    "MaterialExplorerScene.cpp"
    "MaterialExplorerScene.hpp"
)
target_link_libraries(CrispMaterialExplorerScene
    PUBLIC Crisp::PathTracedView
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::ImGuiCameraUtils
    PRIVATE ImGui
)

add_cpp_static_library(CrispOceanScene
    "OceanScene.cpp"
    "OceanScene.hpp"
)
target_link_libraries(CrispOceanScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::MaterialUtils
    PRIVATE Crisp::ImGuiCameraUtils
)
add_cpp_test(CrispMaterialExplorerSceneTest
    "Test/MaterialExplorerSceneRenderTest.cpp")
target_link_libraries(CrispMaterialExplorerSceneTest
    PRIVATE Crisp::MaterialExplorerScene
    PRIVATE Crisp::Renderer
    PRIVATE Crisp::Application
    PRIVATE Crisp::ImageIo)
target_add_test_file(CrispMaterialExplorerSceneTest "Test/Data/material-explorer-path-traced.exr")

add_cpp_test(CrispOceanSceneTest
    "Test/OceanSceneRenderTest.cpp")
target_link_libraries(CrispOceanSceneTest
    PRIVATE Crisp::OceanScene
    PRIVATE Crisp::Renderer
    PRIVATE Crisp::Application
    PRIVATE Crisp::ImageIo)
target_add_test_file(CrispOceanSceneTest "Test/Data/ocean-reference.exr")

target_link_libraries(CrispPbrScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::ImGuiCameraUtils
    PRIVATE Crisp::AtmosphereGui
)

add_cpp_static_library(CrispFluidSimulationScene
    "FluidSimulationScene.cpp"
    "FluidSimulationScene.hpp"
)
target_link_libraries(CrispFluidSimulationScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Models
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::RenderGraphGui
    PRIVATE ImGui
)
add_cpp_test(CrispFluidSimulationSceneTest
    "Test/FluidSimulationSceneTest.cpp")
target_link_libraries(CrispFluidSimulationSceneTest
    PRIVATE Crisp::FluidSimulationScene
    PRIVATE Crisp::Renderer
    PRIVATE Crisp::Application)

add_cpp_static_library(CrispVulkanRayTracingScene
    "EnvironmentLightSampling.cpp"
    "EnvironmentLightSampling.hpp"
    "VulkanRayTracingScene.cpp"
    "VulkanRayTracingScene.hpp"
    "RayTracingSceneParser.cpp"
    "RayTracingSceneParser.hpp"
    "RayTracingSceneData.hpp"
)
target_link_libraries(CrispVulkanRayTracingScene
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::ImGuiCameraUtils
    PRIVATE Crisp::SpvReflection
    PUBLIC Crisp::VulkanDescriptorHeap
    PUBLIC Crisp::RayTracingPipelineBuilder
)

add_cpp_test(CrispBsdfValidationTest
    "Test/BsdfValidationTest.cpp")
set_target_properties(CrispBsdfValidationTest PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_link_libraries(CrispBsdfValidationTest
    PRIVATE Crisp::Renderer
    PRIVATE Crisp::VulkanTestUtils)
target_add_test_shader(CrispBsdfValidationTest "Test/Data/bsdf-validation.comp.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Common/math-constants.part.glsl" "Common/math-constants.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Common/warp.part.glsl" "Common/warp.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/lambertian.part.glsl" "Brdf/lambertian.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/oren-nayar.part.glsl" "Brdf/oren-nayar.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/microfacet.part.glsl" "Brdf/microfacet.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/dielectric.part.glsl" "Brdf/dielectric.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/fresnel.part.glsl" "Brdf/fresnel.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/smooth-conductor.part.glsl" "Brdf/smooth-conductor.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/rough-conductor.part.glsl" "Brdf/rough-conductor.part.glsl")
target_add_test_file(
    CrispBsdfValidationTest "../Shaders/Brdf/rough-dielectric.part.glsl" "Brdf/rough-dielectric.part.glsl")

add_cpp_test(CrispEnvironmentSamplingTest
    "Test/EnvironmentSamplingTest.cpp")
target_link_libraries(CrispEnvironmentSamplingTest
    PRIVATE Crisp::VulkanRayTracingScene
    PRIVATE Crisp::VulkanTestUtils)
target_add_test_shader(CrispEnvironmentSamplingTest "Test/Data/environment-sampling.comp.glsl")
target_add_test_shader(CrispEnvironmentSamplingTest "Test/Data/point-light.comp.glsl")
target_add_test_shader(CrispEnvironmentSamplingTest "Test/Data/directional-light.comp.glsl")
target_add_test_file(
    CrispEnvironmentSamplingTest "../Shaders/Common/math-constants.part.glsl" "Common/math-constants.part.glsl")
target_add_test_file(
    CrispEnvironmentSamplingTest
    "../Shaders/PathTracer/Lights/environment-distribution.part.glsl"
    "PathTracer/Lights/environment-distribution.part.glsl")
target_add_test_file(
    CrispEnvironmentSamplingTest
    "../Shaders/PathTracer/Lights/point-light.part.glsl"
    "PathTracer/Lights/point-light.part.glsl")
target_add_test_file(
    CrispEnvironmentSamplingTest
    "../Shaders/PathTracer/Lights/directional-light.part.glsl"
    "PathTracer/Lights/directional-light.part.glsl")

add_cpp_test(CrispRayTracingSceneParserTest
    "Test/RayTracingSceneParserTest.cpp")
target_link_libraries(CrispRayTracingSceneParserTest
    PRIVATE Crisp::VulkanRayTracingScene)
