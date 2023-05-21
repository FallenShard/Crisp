#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
class Application;

class SceneContainer
{
public:
    SceneContainer(Renderer* renderer, Application* app, const std::string& sceneName);

    static const std::vector<std::string>& getSceneNames();

    void resize(int width, int height);
    void update(float dt);
    void render() const;

    void onSceneSelected(const std::string& sceneName);

    const std::string& getSceneName() const;

private:
    std::unique_ptr<AbstractScene> m_scene;
    std::string m_sceneName;

    Renderer* m_renderer;
    Application* m_application;
};
} // namespace crisp