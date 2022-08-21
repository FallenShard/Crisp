add_cpp_static_library(CrispScenes
    "SceneContainer.cpp"
    "SceneContainer.hpp"
    "AmbientOcclusionScene.cpp"
    "AmbientOcclusionScene.hpp"
    "ClusteredLightingScene.cpp"
    "ClusteredLightingScene.hpp"
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
    "Scene.hpp"
    "ShadowMappingScene.cpp"
    "ShadowMappingScene.hpp"
    "TestScene.cpp"
    "TestScene.hpp"
    "VulkanRayTracingScene.cpp"
    "VulkanRayTracingScene.hpp"
    "WorldScene.cpp"
    "WorldScene.hpp")
target_link_libraries(CrispScenes
    PUBLIC CrispRenderer
    PUBLIC CrispCamera
    PUBLIC CrispLights
    PUBLIC CrispModels
    PUBLIC CrispMeshIo
    PUBLIC CrispMaterialUtils
    PUBLIC Vesper
    PRIVATE CrispApplication)