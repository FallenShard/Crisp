#pragma once

#include <memory>
#include <string>

namespace crisp
{
    class VulkanRenderer;
    class InputDispatcher;
    class Application;
    class Scene;
    class TestScene;

    namespace gui
    {
        class ComboBox;
    }

    class SceneContainer
    {
    public:
        SceneContainer(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app);
        ~SceneContainer();

        void resize(int width, int height);
        void update(float dt);
        void render() const;

        void onSceneSelected(std::string sceneName);

    private:
        std::unique_ptr<Scene> m_scene;

        gui::ComboBox* m_sceneComboBox;
        VulkanRenderer* m_renderer;
        InputDispatcher* m_inputDispatcher;
        Application* m_application;
    };
}