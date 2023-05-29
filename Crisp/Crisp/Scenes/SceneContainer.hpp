#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
class SceneContainer
{
public:
    SceneContainer(Renderer* renderer, Window* window, const std::string& sceneName);

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
    Window* m_window;
};
} // namespace crisp