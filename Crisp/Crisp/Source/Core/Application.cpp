#include "Application.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>

#include <CrispCore/ConsoleUtils.hpp>

#include "Renderer/Renderer.hpp"
#include "Core/Window.hpp"
#include "Core/InputDispatcher.hpp"
#include "Core/SceneContainer.hpp"

#include "IO/FileUtils.hpp"
#include "IO/ImageFileBuffer.hpp"

#include "GUI/RenderSystem.hpp"
#include "GUI/Form.hpp"
#include "GUI/Button.hpp"
#include "GUI/StatusBar.hpp"
#include "GUI/MemoryUsageBar.hpp"
#include "GUI/IntroductionPanel.hpp"

namespace crisp
{
    namespace
    {
        void logFpsToConsole(double frameTime, double fps)
        {
            std::ostringstream stringStream;
            stringStream << std::fixed << std::setprecision(2) << std::setfill('0') << frameTime << " ms"
                << ", " << std::setfill('0') << fps << " fps";

            ConsoleColorizer color(ConsoleColor::Yellow);
            std::cout << stringStream.str() << '\r';
        }
    }

    Application::Application(const ApplicationEnvironment& env)
        : m_frameTimeLogger(1000.0)
    {
        std::cout << "Initializing application...\n";

        m_window   = createWindow();
        m_renderer = createRenderer();

        auto inputDispatcher = m_window->getInputDispatcher();
        inputDispatcher->windowResized.subscribe<&Application::onResize>(this);

        // Create and connect GUI with the mouse
        m_guiForm = std::make_unique<gui::Form>(std::make_unique<gui::RenderSystem>(m_renderer.get()));
        inputDispatcher->mouseMoved.subscribe<&gui::Form::onMouseMoved>(m_guiForm.get());
        inputDispatcher->mouseButtonPressed.subscribe<&gui::Form::onMousePressed>(m_guiForm.get());
        inputDispatcher->mouseButtonReleased.subscribe<&gui::Form::onMouseReleased>(m_guiForm.get());
        inputDispatcher->mouseEntered.subscribe<&gui::Form::onMouseEntered>(m_guiForm.get());
        inputDispatcher->mouseExited.subscribe<&gui::Form::onMouseExited>(m_guiForm.get());
        //m_frameTimeLogger.onLoggerUpdated.subscribe<&logFpsToConsole>();

        auto statusBar = std::make_unique<gui::StatusBar>(m_guiForm.get());
        m_frameTimeLogger.onLoggerUpdated.subscribe<&gui::StatusBar::setFrameTimeAndFps>(statusBar.get());
        m_guiForm->add(std::move(statusBar));

        m_guiForm->add(std::make_unique<gui::MemoryUsageBar>(m_guiForm.get()));
        m_guiForm->add(gui::createIntroPanel(m_guiForm.get(), this), false);

        m_sceneContainer = std::make_unique<SceneContainer>(m_renderer.get(), this);
    }

    Application::~Application()
    {
    }

    void Application::run()
    {
        {
            ConsoleColorizer clr(ConsoleColor::LightGreen);
            std::cout << "Hello world from Crisp! The application is up and running!\n";
        }

        m_renderer->flushResourceUpdates();

        m_frameTimeLogger.restart();
        Timer<std::milli> updateTimer;
        double timeSinceLastUpdate = 0.0;
        while (!m_window->shouldClose())
        {
            auto timeDelta = updateTimer.restart() / 1000.0;
            timeSinceLastUpdate += timeDelta;

            Window::pollEvents();

            while (timeSinceLastUpdate > TimePerFrame)
            {
                if (m_sceneContainer)
                    m_sceneContainer->update(static_cast<float>(TimePerFrame));

                m_guiForm->update(TimePerFrame);

                timeSinceLastUpdate -= TimePerFrame;
            }

            if (m_sceneContainer)
                m_sceneContainer->render();

            m_guiForm->draw();

            m_renderer->drawFrame();

            m_frameTimeLogger.update();
        }

        m_renderer->finish();
    }

    void Application::close()
    {
        m_window->close();
    }

    void Application::onResize(int width, int height)
    {
        std::cout << "New window dims: (" << width << ", " << height << ")\n";

        m_renderer->resize(width, height);

        if (m_sceneContainer)
            m_sceneContainer->resize(width, height);

        if (m_guiForm)
            m_guiForm->resize(width, height);
    }

    SceneContainer* Application::getSceneContainer() const
    {
        return m_sceneContainer.get();
    }

    gui::Form* Application::getForm() const
    {
        return m_guiForm.get();
    }

    Window* Application::getWindow() const
    {
        return m_window.get();
    }

    std::unique_ptr<Window> Application::createWindow()
    {
        auto desktopRes = Window::getDesktopResolution();
        auto size = glm::ivec2(DefaultWindowWidth, DefaultWindowHeight);

        return std::make_unique<Window>((desktopRes - size) / 2, size, Title);
    }

    std::unique_ptr<Renderer> Application::createRenderer()
    {
        auto surfaceCreator = [this](VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface)
        {
            return m_window->createRenderingSurface(instance, allocCallbacks, surface);
        };

        return std::make_unique<Renderer>(surfaceCreator, Window::getVulkanExtensions());
    }
}