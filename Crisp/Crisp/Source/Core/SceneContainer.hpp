#pragma once

#include <memory>
#include <vector>
#include <string>

namespace crisp
{
    class VulkanRenderer;
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
        SceneContainer(VulkanRenderer* renderer, Application* app);
        ~SceneContainer();

        static std::vector<std::string> getSceneNames();
        static const std::string& getDefaultScene();

        void resize(int width, int height);
        void update(float dt);
        void render() const;

        void onSceneSelected(std::string sceneName);

    private:
        std::unique_ptr<Scene> m_scene;

        VulkanRenderer* m_renderer;
        Application* m_application;
    };
}