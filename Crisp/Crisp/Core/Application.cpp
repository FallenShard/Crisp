#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/GUI/ComboBox.hpp>
#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/MemoryUsageBar.hpp>
#include <Crisp/GUI/RenderSystem.hpp>
#include <Crisp/GUI/StatusBar.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/SceneContainer.hpp>

#include <CrispCore/ChromeProfiler.hpp>
#include <CrispCore/Logger.hpp>

namespace crisp
{
namespace
{
auto logger = createLoggerMt("Application");
void logFps(double frameTime, double fps)
{
    logger->debug("{:03.2f} ms, {:03.2f} fps\r", frameTime, fps);
}
} // namespace

Application::Application(const ApplicationEnvironment&)
    : m_accumulatedTime(0.0)
    , m_accumulatedFrames(0.0)
    , m_updatePeriod(1.0)
{
    logger->info("Initializing...");

    m_window = createWindow();
    logger->info("Window opened!");

    m_renderer = std::make_unique<Renderer>(m_window->createSurfaceCallback());
    logger->info("Renderer created!");

    m_sceneContainer = std::make_unique<SceneContainer>(m_renderer.get(), this);
    logger->info("SceneContainer created!");

    m_window->resized.subscribe<&Application::onResize>(this);
    m_window->minimized.subscribe<&Application::onMinimize>(this);
    m_window->restored.subscribe<&Application::onRestore>(this);

    // Create and connect GUI with the mouse
    m_guiForm = std::make_unique<gui::Form>(std::make_unique<gui::RenderSystem>(m_renderer.get()));
    logger->info("GUI Created");
    m_window->mouseMoved.subscribe<&gui::Form::onMouseMoved>(m_guiForm.get());
    m_window->mouseButtonPressed.subscribe<&gui::Form::onMousePressed>(m_guiForm.get());
    m_window->mouseButtonReleased.subscribe<&gui::Form::onMouseReleased>(m_guiForm.get());
    m_window->mouseEntered.subscribe<&gui::Form::onMouseEntered>(m_guiForm.get());
    m_window->mouseExited.subscribe<&gui::Form::onMouseExited>(m_guiForm.get());

    auto comboBox = std::make_unique<gui::ComboBox>(m_guiForm.get());
    comboBox->setId("sceneComboBox");
    comboBox->setPosition({ 0, 0 });
    comboBox->setItems(SceneContainer::getSceneNames());
    comboBox->setWidthHint(200.0f);

    auto statusBar = std::make_unique<gui::StatusBar>(m_guiForm.get());
    onFrameTimeUpdated.subscribe<&gui::StatusBar::setFrameTimeAndFps>(statusBar.get());
    statusBar->addControl(std::move(comboBox));
    m_guiForm->add(std::move(statusBar));

    m_guiForm->add(std::make_unique<gui::MemoryUsageBar>(m_guiForm.get()));
    m_guiForm->processGuiUpdates();
    m_guiForm->printGuiTree();

    auto cb = m_guiForm->getControlById<gui::ComboBox>("sceneComboBox");
    cb->itemSelected.subscribe<&SceneContainer::onSceneSelected>(m_sceneContainer.get());
    cb->selectItem(m_sceneContainer->getDefaultSceneIndex());
    logger->info("Initialized start-up scene");
}

Application::~Application() {}

void Application::run()
{
    m_renderer->flushResourceUpdates(true);

    logger->info("Hello world from Crisp! The application is up and running!");
    Timer<std::chrono::duration<double>> updateTimer;
    double timeSinceLastUpdate = 0.0;
    while (!m_window->shouldClose())
    {
        const double timeDelta = updateTimer.restart();
        updateFrameStatistics(timeDelta);
        timeSinceLastUpdate += timeDelta;

        Window::pollEvents();
        if (m_isMinimized)
            continue;

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
    }

    m_renderer->finish();
}

void Application::close()
{
    m_window->close();
}

void Application::onResize(int width, int height)
{
    logger->info("New window dims: [{}, {}]", width, height);
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
    const glm::ivec2 desktopRes = Window::getDesktopResolution();
    constexpr glm::ivec2 size(DefaultWindowWidth, DefaultWindowHeight);

    return std::make_unique<Window>((desktopRes - size) / 2, size, Title);
}

void Application::updateFrameStatistics(double frameTime)
{
    m_accumulatedTime += frameTime;
    m_accumulatedFrames++;

    if (m_accumulatedTime >= m_updatePeriod)
    {
        const double spillOver = m_accumulatedTime - m_updatePeriod;
        const double spillOverFrac = spillOver / frameTime;

        const double avgTime = m_updatePeriod / (m_accumulatedFrames - spillOverFrac);
        const double avgFrames = 1.0 / avgTime;

        onFrameTimeUpdated(avgTime, avgFrames);

        m_accumulatedTime = spillOver;
        m_accumulatedFrames = spillOverFrac;
    }
}
void Application::onMinimize()
{
    m_isMinimized = true;
}
void Application::onRestore()
{
    m_isMinimized = false;
}
} // namespace crisp
