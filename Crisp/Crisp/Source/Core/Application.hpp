#pragma once

#include "ApplicationEnvironment.hpp"
#include "Core/FrameTimeLogger.hpp"

#include <CrispCore/Timer.hpp>

#include <memory>

namespace crisp
{
    class Window;
    class Renderer;
    class SceneContainer;

    namespace gui
    {
        class Form;
    }

    class Application
    {
    public:
        static constexpr const char* Title = "Crisp";
        static constexpr int DefaultWindowWidth  = 1280;
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

    private:
        std::unique_ptr<Window>   createWindow();
        std::unique_ptr<Renderer> createRenderer();

        FrameTimeLogger<Timer<std::milli>> m_frameTimeLogger;

        std::unique_ptr<Window>    m_window;
        std::unique_ptr<Renderer>  m_renderer;

        std::unique_ptr<gui::Form> m_guiForm;

        std::unique_ptr<SceneContainer> m_sceneContainer;
    };
}