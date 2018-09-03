#pragma once

#include <memory>
#include <vector>
#include <string>

namespace crisp
{
    class Renderer;
    class Application;
    class AbstractScene;

    namespace gui
    {
        class ComboBox;
    }

    class SceneContainer
    {
    public:
        SceneContainer(Renderer* renderer, Application* app);
        ~SceneContainer();

        static std::vector<std::string> getSceneNames();
        static const std::string& getDefaultScene();

        void resize(int width, int height);
        void update(float dt);
        void render() const;

        void onSceneSelected(std::string sceneName);

    private:
        std::unique_ptr<AbstractScene> m_scene;

        Renderer* m_renderer;
        Application* m_application;
    };
}