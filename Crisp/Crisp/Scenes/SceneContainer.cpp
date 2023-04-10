#include <Crisp/Scenes/SceneContainer.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Scenes/AmbientOcclusionScene.hpp>
#include <Crisp/Scenes/ClusteredLightingScene.hpp>
#include <Crisp/Scenes/FluidSimulationScene.hpp>
#include <Crisp/Scenes/GltfViewerScene.hpp>
#include <Crisp/Scenes/NormalMappingScene.hpp>
#include <Crisp/Scenes/OceanScene.hpp>
#include <Crisp/Scenes/PbrScene.hpp>
#include <Crisp/Scenes/RayTracerScene.hpp>
#include <Crisp/Scenes/ShadowMappingScene.hpp>
#include <Crisp/Scenes/TestScene.hpp>
#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <Crisp/Common/Logger.hpp>

namespace crisp
{
namespace
{
std::vector<std::string> sceneNames = {
    "ambient-occlusion",
    "fluid-simulation",
    "shadow-mapping",
    "ray-tracer",
    "pbr",
    "clustered-lighting",
    "normal-mapping",
    "vulkan-ray-tracer",
    "ocean",
    "gltf-viewer",
    "null"};

template <typename... Args>
std::unique_ptr<AbstractScene> createScene(const std::string& name, Args&&... args)
{
    if (name == sceneNames[0])
        return std::make_unique<AmbientOcclusionScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[1])
        return std::make_unique<FluidSimulationScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[2])
        return std::make_unique<ShadowMappingScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[3])
        return std::make_unique<RayTracerScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[4])
        return std::make_unique<PbrScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[5])
        return std::make_unique<ClusteredLightingScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[6])
        return std::make_unique<NormalMappingScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[7])
        return std::make_unique<VulkanRayTracingScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[8])
        return std::make_unique<OceanScene>(std::forward<Args>(args)...);
    else if (name == sceneNames[9])
        return std::make_unique<GltfViewerScene>(std::forward<Args>(args)...);
    else
    {
        spdlog::warn("Scene with the name {} is invalid/disabled", name);
        return std::make_unique<TestScene>(std::forward<Args>(args)...);
    }
}
} // namespace

SceneContainer::SceneContainer(Renderer* renderer, Application* app, const std::string& sceneName)
    : m_renderer(renderer)
    , m_application(app)
{
    m_scene = createScene(sceneName, m_renderer, m_application);
}

SceneContainer::~SceneContainer() {}

std::vector<std::string> SceneContainer::getSceneNames()
{
    return sceneNames;
}

void SceneContainer::update(float dt)
{
    if (m_scene)
        m_scene->update(dt);
}

void SceneContainer::render() const
{
    if (m_scene)
    {
        m_scene->render();
        m_scene->renderGui();
    }
}

void SceneContainer::onSceneSelected(const std::string& sceneName)
{
    m_renderer->finish();
    m_renderer->setSceneImageView(nullptr, 0);
    m_scene.reset();
    m_scene = createScene(sceneName, m_renderer, m_application);
}

void SceneContainer::resize(int width, int height)
{
    if (m_scene)
        m_scene->resize(width, height);
}
} // namespace crisp