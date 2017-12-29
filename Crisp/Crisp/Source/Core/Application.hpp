#pragma once

#define NOMINMAX

#include <memory>

#include <CrispCore/Timer.hpp>

#include "ApplicationEnvironment.hpp"
#include "Core/FrameTimeLogger.hpp"

namespace crisp
{
    class Window;
    class InputDispatcher;
    class VulkanRenderer;
    class BackgroundImage;
    class RayTracedImage;
    class SceneContainer;

    namespace gui
    {
        class Form;
    }

    class Application
    {
    public:
        static constexpr const char* Title = "Crisp";
        static constexpr double TimePerFrame = 1.0 / 60.0;
        static constexpr int DefaultWindowWidth  = 1280;
        static constexpr int DefaultWindowHeight = 720;

        Application(ApplicationEnvironment* environment);
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
        void createWindow();
        void createRenderer();

        FrameTimeLogger<Timer<std::milli>> m_frameTimeLogger;

        std::unique_ptr<Window>          m_window;
        std::unique_ptr<VulkanRenderer>  m_renderer;

        std::unique_ptr<BackgroundImage> m_backgroundImage;

        std::unique_ptr<gui::Form> m_guiForm;

        std::unique_ptr<SceneContainer> m_sceneContainer;
    };
}