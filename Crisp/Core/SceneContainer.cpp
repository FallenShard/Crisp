#include "SceneContainer.hpp"

#include "Scenes/TestScene.hpp"
#include "Scenes/FluidSimulationScene.hpp"
#include "Scenes/ShadowMappingScene.hpp"
#include "Scenes/AmbientOcclusionScene.hpp"
#include "Scenes/RayTracerScene.hpp"
#include "Scenes/PbrScene.hpp"
#include "Scenes/ClusteredLightingScene.hpp"
#include "Scenes/NormalMappingScene.hpp"
#include "Scenes/VulkanRayTracingScene.hpp"
#include "Scenes/OceanScene.hpp"
#include "Application.hpp"
#include "GUI/Form.hpp"
#include "GUI/ComboBox.hpp"

#include <spdlog/spdlog.h>

namespace crisp
{
    namespace
    {
        std::vector<std::string> sceneNames =
        {
            "Ambient Occlusion",
            "Liquid Simulation",
            "Shadow Mapping",
            "Ray Tracer",
            "Physically-based Rendering",
            "Clustered Lighting",
            "Normal Mapping",
            "VulkanRayTracingScene",
            "Ocean",
            "Null"
        };

        template <typename ...Args>
        std::unique_ptr<AbstractScene> createScene(const std::string& name, Args&&... args)
        {
            if      (name == sceneNames[0]) return std::make_unique<AmbientOcclusionScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[1]) return std::make_unique<FluidSimulationScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[2]) return std::make_unique<ShadowMappingScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[3]) return std::make_unique<RayTracerScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[4]) return std::make_unique<PbrScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[5]) return std::make_unique<ClusteredLightingScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[6]) return std::make_unique<NormalMappingScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[7]) return std::make_unique<VulkanRayTracingScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[8]) return std::make_unique<OceanScene>(std::forward<Args>(args)...);
            else {
                spdlog::warn("Scene with the name {} is invalid/disabled", name);
                return std::make_unique<TestScene>(std::forward<Args>(args)...);
            }
        }
    }

    SceneContainer::SceneContainer(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_application(app)
    {
        m_defaultSceneIndex = ApplicationEnvironment::getArgument<uint32_t>("scene");
    }

    SceneContainer::~SceneContainer()
    {
    }

    std::vector<std::string> SceneContainer::getSceneNames()
    {
        return sceneNames;
    }

    const std::string& SceneContainer::getDefaultScene() const
    {
        return sceneNames.at(m_defaultSceneIndex);
    }

    std::size_t SceneContainer::getDefaultSceneIndex() const
    {
        return m_defaultSceneIndex;
    }

    void SceneContainer::update(float dt)
    {
        if (m_scene)
            m_scene->update(dt);
    }

    void SceneContainer::render() const
    {
        if (m_scene)
            m_scene->render();
    }

    void SceneContainer::onSceneSelected(const std::string& sceneName)
    {
        m_renderer->finish();
        m_scene.reset();
        m_scene = createScene(sceneName, m_renderer, m_application);
    }

    void SceneContainer::resize(int width, int height)
    {
        if (m_scene)
            m_scene->resize(width, height);
    }
}