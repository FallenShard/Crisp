#pragma once

#include <memory>
#include <string>
#include <vector>

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
    const std::string& getDefaultScene() const;
    std::size_t getDefaultSceneIndex() const;

    void resize(int width, int height);
    void update(float dt);
    void render() const;

    void onSceneSelected(const std::string& sceneName);

private:
    std::unique_ptr<AbstractScene> m_scene;

    Renderer* m_renderer;
    Application* m_application;

    uint32_t m_defaultSceneIndex;
};
} // namespace crisp