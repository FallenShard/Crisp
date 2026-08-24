add_cpp_static_library(CrispScenes
    "SceneContainer.cpp"
    "SceneContainer.hpp"

    "AtmosphereScene.cpp"
    "AtmosphereScene.hpp"

    # "ClusteredLightingScene.cpp"
    # "ClusteredLightingScene.hpp"

    # "ShadowMappingScene.cpp"
    # "ShadowMappingScene.hpp"
)
target_link_libraries(CrispScenes
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights

    PRIVATE Crisp::PbrScene
    PRIVATE Crisp::MaterialExplorerScene
    PRIVATE Crisp::GltfViewerScene
    PRIVATE Crisp::OceanScene
    PRIVATE Crisp::FluidSimulationScene
    PRIVATE Crisp::VulkanRayTracingScene

    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PUBLIC Crisp::PathTracer

    PRIVATE Crisp::Application

    PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::ImGuiCameraUtils
    PRIVATE Crisp::AtmosphereGui
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

add_cpp_static_library(CrispMaterialExplorerScene
    "MaterialExplorerScene.cpp"
    "MaterialExplorerScene.hpp"
)
target_link_libraries(CrispMaterialExplorerScene
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
