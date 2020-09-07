#include "Application.hpp"

#include "Renderer/Renderer.hpp"
#include "Core/Window.hpp"
#include "Core/SceneContainer.hpp"

#include "GUI/RenderSystem.hpp"
#include "GUI/Form.hpp"
#include "GUI/StatusBar.hpp"
#include "GUI/MemoryUsageBar.hpp"
#include "GUI/ComboBox.hpp"
#include "GUI/IntroductionPanel.hpp"
#include "GUI/Label.hpp"

#include <CrispCore/Log.hpp>

namespace crisp
{
    namespace
    {
        void logFpsToConsole(double frameTime, double fps)
        {
            logDebug("{:03.2f} ms, {:03.2f} fps\r", frameTime, fps);
        }

        static constexpr const char* LogTag = "Application";
    }

    Application::Application(const ApplicationEnvironment&)
        : m_frameTimeLogger(1000.0)
    {
        logTagInfo(LogTag, "Initializing application...\n");
        m_window   = createWindow();
        m_renderer = createRenderer();

        m_sceneContainer = std::make_unique<SceneContainer>(m_renderer.get(), this);

        m_window->resized.subscribe<&Application::onResize>(this);

        // Create and connect GUI with the mouse
        m_guiForm = std::make_unique<gui::Form>(std::make_unique<gui::RenderSystem>(m_renderer.get()));
        m_window->mouseMoved.subscribe<&gui::Form::onMouseMoved>(m_guiForm.get());
        m_window->mouseButtonPressed.subscribe<&gui::Form::onMousePressed>(m_guiForm.get());
        m_window->mouseButtonReleased.subscribe<&gui::Form::onMouseReleased>(m_guiForm.get());
        m_window->mouseEntered.subscribe<&gui::Form::onMouseEntered>(m_guiForm.get());
        m_window->mouseExited.subscribe<&gui::Form::onMouseExited>(m_guiForm.get());
        //m_frameTimeLogger.onLoggerUpdated.subscribe<&logFpsToConsole>();

        auto comboBox = std::make_unique<gui::ComboBox>(m_guiForm.get());
        comboBox->setId("sceneComboBox");
        comboBox->setPosition({ 0, 0 });
        comboBox->setItems(SceneContainer::getSceneNames());
        comboBox->setWidthHint(200.0f);
        auto cb = comboBox.get();

        auto statusBar = std::make_unique<gui::StatusBar>(m_guiForm.get());
        statusBar->addControl(std::move(comboBox));
        m_frameTimeLogger.onLoggerUpdated.subscribe<&gui::StatusBar::setFrameTimeAndFps>(statusBar.get());
        m_guiForm->add(std::move(statusBar));

        m_guiForm->add(std::make_unique<gui::MemoryUsageBar>(m_guiForm.get()));
        //m_guiForm->add(gui::createIntroPanel(m_guiForm.get(), this), false);
        m_guiForm->processGuiUpdates();
        m_guiForm->printGuiTree();

        cb->itemSelected.subscribe<&SceneContainer::onSceneSelected>(m_sceneContainer.get());
        cb->selectItem(SceneContainer::getDefaultSceneIndex());
    }

    Application::~Application()
    {
    }

    void Application::run()
    {
        logTagInfo(LogTag, "Hello world from Crisp! The application is up and running!\n");
        logTagInfo(LogTag, "Use WASD to move and hold down right click to rotate the camera.\n");

        m_renderer->flushResourceUpdates(true);

        m_frameTimeLogger.restart();
        Timer<std::milli> updateTimer;
        double timeSinceLastUpdate = 0.0;
        while (!m_window->shouldClose())
        {
            auto timeDelta = updateTimer.restart() / 1000.0;
            timeSinceLastUpdate += timeDelta;

            Window::pollEvents();
            m_guiForm->processGuiUpdates();

            while (timeSinceLastUpdate > TimePerFrame)
            {
                m_sceneContainer->update(static_cast<float>(TimePerFrame));

                m_guiForm->update(TimePerFrame);

                timeSinceLastUpdate -= TimePerFrame;
            }

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
        logInfo("New window dims: [{}, {}]\n", width, height);
        if (width == 0 || height == 0)
            return;

        m_renderer->resize(width, height);

        m_sceneContainer->resize(width, height);
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
        glm::ivec2 desktopRes = Window::getDesktopResolution();
        glm::ivec2 size(DefaultWindowWidth, DefaultWindowHeight);

        return std::make_unique<Window>((desktopRes - size) / 2, size, Title);
    }

    std::unique_ptr<Renderer> Application::createRenderer()
    {
        auto surfaceCreator = [this](VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface)
        {
            return m_window->createRenderingSurface(instance, allocCallbacks, surface);
        };

        return std::make_unique<Renderer>(surfaceCreator, ApplicationEnvironment::getRequiredVulkanExtensions(), ApplicationEnvironment::getResourcesPath());
    }
}