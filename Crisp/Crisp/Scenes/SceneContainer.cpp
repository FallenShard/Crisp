#include <Crisp/Scenes/SceneContainer.hpp>

#include <Crisp/Scenes/AtmosphereScene.hpp>
#include <Crisp/Scenes/FluidSimulationScene.hpp>
#include <Crisp/Scenes/OceanScene.hpp>
#include <Crisp/Scenes/PbrScene.hpp>
#include <Crisp/Scenes/TestScene.hpp>
#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerSt("SceneContainer");

std::unique_ptr<Scene> createScene(
    const std::string& name,
    Renderer* renderer,
    Window* window,
    const std::filesystem::path& outputDir,
    const nlohmann::json& args) {
    CRISP_LOGI("Creating Scene: {}", name);
    if (name == "fluid-simulation") {
        return std::make_unique<FluidSimulationScene>(renderer, window);
    }
    if (name == "pbr") {
        return std::make_unique<PbrScene>(renderer, window, args);
    }
    if (name == "vulkan-ray-tracer") {
        return std::make_unique<VulkanRayTracingScene>(renderer, window, outputDir, args);
    }
    if (name == "ocean") {
        return std::make_unique<OceanScene>(renderer, window);
    }
    if (name == "atmosphere") {
        return std::make_unique<AtmosphereScene>(renderer, window);
    }

    logger->warn("Scene with the name {} is invalid/disabled", name);
    return std::make_unique<TestScene>(renderer, window);
}
} // namespace

SceneContainer::SceneContainer(
    Renderer* renderer,
    Window* window,
    std::filesystem::path outputDir,
    const std::string& sceneName,
    nlohmann::json scenes)
    : m_outputDir(std::move(outputDir))
    , m_sceneName(sceneName)
    , m_scenes(std::move(scenes))
    , m_renderer(renderer)
    , m_window(window) {
    m_sceneNames.reserve(m_scenes.size());
    for (const auto& item : m_scenes.items()) {
        m_sceneNames.push_back(item.key());
    }
    m_scene = createScene(sceneName, m_renderer, m_window, m_outputDir, m_scenes.at(sceneName));
}

const std::vector<std::string>& SceneContainer::getSceneNames() const {
    return m_sceneNames;
}

void SceneContainer::update(const UpdateParams& updateParams) {
    if (m_scene) {
        m_scene->update(updateParams);
    }
}

void SceneContainer::render(const FrameContext& frameContext) const {
    if (m_scene) {
        m_scene->drawGui();
        m_scene->render(frameContext);
    }
}

void SceneContainer::onSceneSelected(const std::string& sceneName) {
    if (sceneName == m_sceneName) {
        return;
    }
    m_pendingSceneName = sceneName;
}

void SceneContainer::applyPendingSceneSelection() {
    if (!m_pendingSceneName) {
        return;
    }
    const std::string sceneName{std::move(*m_pendingSceneName)};
    m_pendingSceneName.reset();

    m_renderer->flushResourceUpdates(true);
    m_renderer->finish();
    m_renderer->setSceneImageView(nullptr);
    m_scene.reset();

    m_renderer->collectAllDeferredResources();

    auto scene = createScene(sceneName, m_renderer, m_window, m_outputDir, m_scenes.at(sceneName));
    scene->update({
        .frameIdx = static_cast<uint32_t>(m_renderer->getCurrentFrameIndex()),
        .frameInFlightIdx = m_renderer->getCurrentVirtualFrameIndex(),
        .dt = 0.0f,
        .totalTimeSec = 0.0f,
    });
    m_renderer->flushResourceUpdates(true);

    m_scene = std::move(scene);
    m_sceneName = sceneName;
}

const std::string& SceneContainer::getSceneName() const {
    return m_sceneName;
}

void SceneContainer::resize(int width, int height) {
    if (m_scene) {
        m_scene->resize(width, height);
    }
}
} // namespace crisp
