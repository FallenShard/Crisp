#pragma once

#include <memory>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Event.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/SceneContainer.hpp>

namespace crisp {
class Application {
public:
    static constexpr const char* kTitle = "Crisp";
    static constexpr int32_t kDefaultWindowWidth = 1920;
    static constexpr int32_t kDefaultWindowHeight = 1080;
    static constexpr glm::ivec2 kDefaultWindowSize{kDefaultWindowWidth, kDefaultWindowHeight};

    static constexpr uint32_t kDesiredFramesPerSecond = 240;
    static constexpr double kTimePerFrame = 1.0 / kDesiredFramesPerSecond;

    explicit Application(const ApplicationEnvironment& environment);
    ~Application();

    Application(const Application& other) = delete;
    Application(Application&& other) = delete;
    Application& operator=(const Application& other) = delete;
    Application& operator=(Application&& other) = delete;

    void run();
    void close();

    Window& getWindow();

    Event<double, double> onFrameTimeUpdated;

private:
    void updateFrameStatistics(double frameTime);
    void onMinimize();
    void onRestore();
    void onResize(int width, int height);
    void resizeIfNeeded();

    void drawGui();

    Window m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<SceneContainer> m_sceneContainer;

    double m_accumulatedTime{0.0};
    double m_accumulatedFrames{0.0};
    double m_updatePeriod{1.0};

    double m_avgFrameTimeMs{0.0};
    double m_avgFps{0.0};

    bool m_isMinimized{false};
    bool m_isResizing{false};

    std::filesystem::path m_outputDir;
};
} // namespace crisp