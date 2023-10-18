add_cpp_static_library(CrispScenes
    "SceneContainer.cpp"
    "SceneContainer.hpp"
    "AmbientOcclusionScene.cpp"
    "AmbientOcclusionScene.hpp"
    "AtmosphereScene.cpp"
    "AtmosphereScene.hpp"
    "ClusteredLightingScene.cpp"
    "ClusteredLightingScene.hpp"
    "GltfViewerScene.cpp"
    "GltfViewerScene.hpp"
    "FluidSimulationScene.cpp"
    "FluidSimulationScene.hpp"
    "NormalMappingScene.cpp"
    "NormalMappingScene.hpp"
    "OceanScene.cpp"
    "OceanScene.hpp"
    "PbrScene.cpp"
    "PbrScene.hpp"
    "RaytracedImage.cpp"
    "RaytracedImage.hpp"
    "RayTracerScene.cpp"
    "RayTracerScene.hpp"
    "Scene.cpp"
    "Scene.hpp"
    "ShadowMappingScene.cpp"
    "ShadowMappingScene.hpp"
    "TestScene.cpp"
    "TestScene.hpp"
    "VulkanRayTracingScene.cpp"
    "VulkanRayTracingScene.hpp"
)
target_link_libraries(CrispScenes
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Camera
    PUBLIC Crisp::Lights
    PUBLIC Crisp::Models
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::MaterialUtils
    PUBLIC Crisp::JsonUtils
    PUBLIC Crisp::PathTracer
    PUBLIC Crisp::RayTracingPipelineBuilder
    PRIVATE Crisp::Application
    PRIVATE Crisp::RenderGraph
)
