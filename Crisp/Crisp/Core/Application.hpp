#pragma once

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/GUI/Form.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/SceneContainer.hpp>

#include <Crisp/Event.hpp>
#include <Crisp/Timer.hpp>

#include <memory>

namespace crisp
{

class Application
{
public:
    static constexpr const char* Title = "Crisp";
    static constexpr int DefaultWindowWidth = 1280;
    static constexpr int DefaultWindowHeight = 720;

    static constexpr uint32_t DesiredFramesPerSecond = 144;
    static constexpr double TimePerFrame = 1.0 / DesiredFramesPerSecond;

    Application(const ApplicationEnvironment& environment);
    ~Application();

    Application(const Application& other) = delete;
    Application(Application&& other) = delete;
    Application& operator=(const Application& other) = delete;
    Application& operator=(Application&& other) = delete;

    void run();
    void close();

    void onResize(int width, int height);

    gui::Form* getForm() const;
    Window* getWindow() const;
    SceneContainer* getSceneContainer() const;

    Event<double, double> onFrameTimeUpdated;

private:
    std::unique_ptr<Window> createWindow();
    void updateFrameStatistics(double frameTime);
    void onMinimize();
    void onRestore();

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;

    std::unique_ptr<gui::Form> m_guiForm;

    std::unique_ptr<SceneContainer> m_sceneContainer;

    double m_accumulatedTime;
    double m_accumulatedFrames;
    double m_updatePeriod;

    bool m_isMinimized{false};
};
} // namespace crisp