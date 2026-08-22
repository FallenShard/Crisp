#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Crisp/Core/Window.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/Scene.hpp>

namespace crisp {
class SceneContainer {
public:
    SceneContainer(
        Renderer* renderer,
        Window* window,
        std::filesystem::path outputDir,
        const std::string& sceneName,
        nlohmann::json scenes);

    const std::vector<std::string>& getSceneNames() const;

    void resize(int width, int height);
    void update(const UpdateParams& updateParams);
    void render(const FrameContext& frameContext) const;

    void onSceneSelected(const std::string& sceneName);
    void applyPendingSceneSelection();

    const std::string& getSceneName() const;

private:
    std::filesystem::path m_outputDir;

    std::unique_ptr<Scene> m_scene;
    std::string m_sceneName;
    std::optional<std::string> m_pendingSceneName;
    nlohmann::json m_scenes;
    std::vector<std::string> m_sceneNames;

    Renderer* m_renderer;
    Window* m_window;
};
} // namespace crisp
