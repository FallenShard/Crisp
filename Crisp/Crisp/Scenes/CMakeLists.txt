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

add_cpp_static_library(CrispPathTracer
    "PathTracer.cpp"
    "PathTracer.hpp"
)
target_link_libraries(CrispPathTracer
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Geometry
    PUBLIC Crisp::VulkanDescriptorHeap
    PUBLIC Crisp::RayTracingPipelineBuilder
)

add_cpp_static_library(CrispPathTracedView
    "PathTracedView.cpp"
    "PathTracedView.hpp"
)
target_link_libraries(CrispPathTracedView
    PUBLIC Crisp::PathTracer
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
add_cpp_test(
    CrispMaterialExplorerSceneTest
    "Test/MaterialExplorerSceneRenderTest.cpp"
    DEPS
    Crisp::MaterialExplorerScene
    Crisp::Renderer
    Crisp::Application
    Crisp::ImageIo
    ASSET_DIR
    "Test/Data"
)

add_cpp_test(
    CrispOceanSceneTest
    "Test/OceanSceneRenderTest.cpp"
    DEPS
    Crisp::OceanScene
    Crisp::Renderer
    Crisp::Application
    Crisp::ImageIo
    ASSET_DIR
    "Test/Data"
)

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
)
target_link_libraries(CrispVulkanRayTracingScene
    PUBLIC Crisp::PathTracer
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

add_cpp_test(
    CrispBsdfValidationTest
    "Test/BsdfValidationTest.cpp"
    DEPS
    Crisp::Renderer
    Crisp::VulkanTestUtils
    Crisp::ShaderCompiler
    Crisp::UniqueTemporaryFile
    ASSET_DIR
    "Test/Data"
)

add_cpp_test(
    CrispEnvironmentSamplingTest
    "Test/EnvironmentSamplingTest.cpp"
    DEPS
    Crisp::VulkanRayTracingScene
    Crisp::VulkanTestUtils
    Crisp::ShaderCompiler
    Crisp::UniqueTemporaryFile
    ASSET_DIR
    "Test/Data"
)

add_cpp_test(CrispRayTracingSceneParserTest
    "Test/RayTracingSceneParserTest.cpp")
target_link_libraries(CrispRayTracingSceneParserTest
    PRIVATE Crisp::VulkanRayTracingScene)
