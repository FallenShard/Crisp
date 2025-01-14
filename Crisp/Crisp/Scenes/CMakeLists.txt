add_cpp_static_library(CrispScenes
    "SceneContainer.cpp"
    "SceneContainer.hpp"

    # "AmbientOcclusionScene.cpp"
    # "AmbientOcclusionScene.hpp"
    # "AtmosphereScene.cpp"
    # "AtmosphereScene.hpp"
    # "ClusteredLightingScene.cpp"
    # "ClusteredLightingScene.hpp"
    # "GltfViewerScene.cpp"
    # "GltfViewerScene.hpp"
    # "FluidSimulationScene.cpp"
    # "FluidSimulationScene.hpp"
    # "NormalMappingScene.cpp"
    # "NormalMappingScene.hpp"
    # "OceanScene.cpp"
    # "OceanScene.hpp"
    # "PbrScene.cpp"
    # "PbrScene.hpp"
    # "RaytracedImage.cpp"
    # "RaytracedImage.hpp"
    # "RayTracerScene.cpp"
    # "RayTracerScene.hpp"

    # "ShadowMappingScene.cpp"
    # "ShadowMappingScene.hpp"

    # "VulkanRayTracingScene.cpp"
    # "VulkanRayTracingScene.hpp"
    # "RayTracingSceneParser.cpp"
    # "RayTracingSceneParser.hpp"
)
target_link_libraries(CrispScenes
    PUBLIC Crisp::Scene
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights

    PRIVATE Crisp::PbrScene

    # PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PUBLIC Crisp::PathTracer
    PUBLIC Crisp::RayTracingPipelineBuilder
    PRIVATE Crisp::Application

    # PRIVATE Crisp::RenderGraph
    # PRIVATE Crisp::RenderGraphGui
    PRIVATE Crisp::ImGuiCameraUtils
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
)
