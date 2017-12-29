#include "SceneContainer.hpp"

#include "Scenes/TestScene.hpp"
#include "Scenes/FluidSimulationScene.hpp"
#include "Scenes/ShadowMappingScene.hpp"
#include "Scenes/AmbientOcclusionScene.hpp"
#include "Scenes/RayTracerScene.hpp"
#include "Application.hpp"
#include "GUI/Form.hpp"
#include "GUI/ComboBox.hpp"

namespace crisp
{
    namespace
    {
        template <typename ...Args>
        std::unique_ptr<Scene> createScene(const std::string& name, Args&&... args)
        {
            if (name == "Liquid Simulation")
                return std::make_unique<FluidSimulationScene>(std::forward<Args>(args)...);
            else if (name == "Shadow Mapping")
                return std::make_unique<ShadowMappingScene>(std::forward<Args>(args)...);
            else if (name == "Ambient Occlusion")
                return std::make_unique<AmbientOcclusionScene>(std::forward<Args>(args)...);
            else if (name == "Ray Tracer")
                return std::make_unique<RayTracerScene>(std::forward<Args>(args)...);
            else
                return std::make_unique<TestScene>(std::forward<Args>(args)...);
        }
    }

    SceneContainer::SceneContainer(VulkanRenderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_application(app)
    {
    }

    SceneContainer::~SceneContainer()
    {
    }

    std::vector<std::string> SceneContainer::getSceneNames()
    {
        return 
        {
            "Ambient Occlusion",
            "Liquid Simulation",
            "Shadow Mapping",
            "Test"
        };
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
        m_scene = std::move(createScene(sceneName, m_renderer, m_application));
    }

    void SceneContainer::resize(int width, int height)
    {
        if (m_scene)
            m_scene->resize(width, height);
    }
}