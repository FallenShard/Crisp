#include "SceneContainer.hpp"

#include "Scenes/TestScene.hpp"
#include "Scene.hpp"
#include "Application.hpp"
#include "GUI/Form.hpp"
#include "GUI/ComboBox.hpp"

namespace crisp
{
    namespace
    {
        template <typename ...Args>
        std::unique_ptr<Scene> createScene(const std::string& name, Args... args)
        {
            if (name == "liquidSimluation")
                return std::make_unique<Scene>(args...);
            else
                return std::make_unique<Scene>(args...);
        }
    }

    SceneContainer::SceneContainer(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app)
        : m_renderer(renderer)
        , m_inputDispatcher(inputDispatcher)
        , m_application(app)
    {
        m_sceneComboBox = app->getForm()->getControlById<gui::ComboBox>("sceneComboBox");
        m_sceneComboBox->itemSelected.subscribe<SceneContainer, &SceneContainer::onSceneSelected>(this);
    }

    SceneContainer::~SceneContainer()
    {
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
        //m_application->getForm()->postGuiUpdate([this, sceneName] {
        //    m_scene.reset();
        //    m_renderer->finish();
        //    m_scene = std::move(createScene(sceneName, m_renderer, m_inputDispatcher, m_application));
        //});
        
        std::cout << "Selected: " << sceneName << std::endl;
    }

    void SceneContainer::resize(int width, int height)
    {
        if (m_scene)
            m_scene->resize(width, height);
    }
}