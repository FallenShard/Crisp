#include <Crisp/Scenes/SceneContainer.hpp>

#include <Crisp/Scenes/AmbientOcclusionScene.hpp>
#include <Crisp/Scenes/AtmosphereScene.hpp>
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

#include <Crisp/Core/Logger.hpp>

namespace crisp
{
namespace
{
const auto logger = createLoggerSt("SceneContainer");

const std::vector<std::string> kSceneNames = {
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
    "atmosphere",
    "null"};

template <typename... Args>
std::unique_ptr<Scene> createScene(const std::string& name, Args&&... args)
{
    logger->info("Creating Scene: {}", name);
    if (name == kSceneNames[0])
    {
        return std::make_unique<AmbientOcclusionScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[1])
    {
        return std::make_unique<FluidSimulationScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[2])
    {
        return std::make_unique<ShadowMappingScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[3])
    {
        return std::make_unique<RayTracerScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[4])
    {
        return std::make_unique<PbrScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[5])
    {
        return std::make_unique<ClusteredLightingScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[6])
    {
        return std::make_unique<NormalMappingScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[7])
    {
        return std::make_unique<VulkanRayTracingScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[8])
    {
        return std::make_unique<OceanScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[9])
    {
        return std::make_unique<GltfViewerScene>(std::forward<Args>(args)...);
    }
    if (name == kSceneNames[10])
    {
        return std::make_unique<AtmosphereScene>(std::forward<Args>(args)...);
    }

    spdlog::warn("Scene with the name {} is invalid/disabled", name);
    return std::make_unique<TestScene>(std::forward<Args>(args)...);
}
} // namespace

SceneContainer::SceneContainer(Renderer* renderer, Window* window, const std::string& sceneName)
    : m_sceneName(sceneName)
    , m_renderer(renderer)
    , m_window(window)
{
    m_scene = createScene(sceneName, m_renderer, m_window);
}

const std::vector<std::string>& SceneContainer::getSceneNames()
{
    return kSceneNames;
}

void SceneContainer::update(float dt)
{
    if (m_scene)
    {
        m_scene->update(dt);
    }
}

void SceneContainer::render() const
{
    if (m_scene)
    {
        m_scene->renderGui();
        m_scene->render();
    }
}

void SceneContainer::onSceneSelected(const std::string& sceneName)
{
    m_renderer->finish();
    m_renderer->setSceneImageView(nullptr, 0);
    m_scene.reset();
    m_scene = createScene(sceneName, m_renderer, m_window);
    m_sceneName = sceneName;
}

const std::string& SceneContainer::getSceneName() const
{
    return m_sceneName;
}

void SceneContainer::resize(int width, int height)
{
    if (m_scene)
    {
        m_scene->resize(width, height);
    }
}
} // namespace crisp