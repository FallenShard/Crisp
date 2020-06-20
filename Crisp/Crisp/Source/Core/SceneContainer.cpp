#include "SceneContainer.hpp"

#include "Scenes/TestScene.hpp"
#include "Scenes/FluidSimulationScene.hpp"
#include "Scenes/ShadowMappingScene.hpp"
#include "Scenes/AmbientOcclusionScene.hpp"
#include "Scenes/RayTracerScene.hpp"
#include "Scenes/PbrScene.hpp"
#include "Scenes/ClusteredLightingScene.hpp"
#include "Application.hpp"
#include "GUI/Form.hpp"
#include "GUI/ComboBox.hpp"

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
            "Test"
        };

        static constexpr std::size_t DefaultSceneIndex = 2;

        template <typename ...Args>
        std::unique_ptr<AbstractScene> createScene(const std::string& name, Args&&... args)
        {
            if      (name == sceneNames[0]) return std::make_unique<AmbientOcclusionScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[1]) return std::make_unique<FluidSimulationScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[2]) return std::make_unique<ShadowMappingScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[3]) return std::make_unique<RayTracerScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[4]) return std::make_unique<PbrScene>(std::forward<Args>(args)...);
            else if (name == sceneNames[5]) return std::make_unique<ClusteredLightingScene>(std::forward<Args>(args)...);
            else                            return std::make_unique<TestScene>(std::forward<Args>(args)...);
        }
    }

    SceneContainer::SceneContainer(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_application(app)
    {
    }

    SceneContainer::~SceneContainer()
    {
    }

    std::vector<std::string> SceneContainer::getSceneNames()
    {
        return sceneNames;
    }

    const std::string& SceneContainer::getDefaultScene()
    {
        return sceneNames.at(DefaultSceneIndex);
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

    void SceneContainer::onSceneSelected(std::string sceneName)
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