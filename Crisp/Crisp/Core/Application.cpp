#include <Crisp/Core/Application.hpp>

#include <Crisp/GUI/ComboBox.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/MemoryUsageBar.hpp>
#include <Crisp/GUI/RenderSystem.hpp>
#include <Crisp/GUI/StatusBar.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/GUI/ImGuiUtils.hpp>
#include <Crisp/Utils/ChromeProfiler.hpp>

#include <imgui.h>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("Application");

void logFps(double frameTime, double fps)
{
    logger->debug("{:03.2f} ms, {:03.2f} fps\r", frameTime, fps);
}

Window createWindow(const char* title, const glm::ivec2& size)
{
    return Window((Window::getDesktopResolution() - size) / 2, size, title);
}

AssetPaths createAssetPaths(const ApplicationEnvironment& environment)
{
    AssetPaths assetPaths{};
    assetPaths.shaderSourceDir = environment.getShaderSourceDirectory();
    assetPaths.resourceDir = environment.getResourcesPath();
    assetPaths.spvShaderDir = environment.getResourcesPath() / "Shaders";
    return assetPaths;
}

} // namespace

Application::Application(const ApplicationEnvironment& environment)
    : m_window(createWindow(Title, DefaultWindowSize))
{
    m_renderer = std::make_unique<Renderer>(
        environment.getRequiredVulkanInstanceExtensions(),
        m_window.createSurfaceCallback(),
        createAssetPaths(environment),
        environment.getParameters().enableRayTracingExtension);
    logger->trace("Renderer created!");

    m_window.resized.subscribe<&Application::onResize>(this);
    m_window.minimized.subscribe<&Application::onMinimize>(this);
    m_window.restored.subscribe<&Application::onRestore>(this);

    //// Create and connect GUI with the mouse
    // m_guiForm = std::make_unique<gui::Form>(std::make_unique<gui::RenderSystem>(m_renderer.get()));
    // logger->trace("GUI created!");
    // m_window.mouseMoved.subscribe<&gui::Form::onMouseMoved>(m_guiForm.get());
    // m_window.mouseButtonPressed.subscribe<&gui::Form::onMousePressed>(m_guiForm.get());
    // m_window.mouseButtonReleased.subscribe<&gui::Form::onMouseReleased>(m_guiForm.get());
    // m_window.mouseEntered.subscribe<&gui::Form::onMouseEntered>(m_guiForm.get());
    // m_window.mouseExited.subscribe<&gui::Form::onMouseExited>(m_guiForm.get());

    // auto comboBox = std::make_unique<gui::ComboBox>(m_guiForm.get());
    // comboBox->setId("sceneComboBox");
    // comboBox->setPosition({0, 0});
    // comboBox->setItems(SceneContainer::getSceneNames());
    // comboBox->setWidthHint(200.0f);

    // auto statusBar = std::make_unique<gui::StatusBar>(m_guiForm.get());
    // onFrameTimeUpdated.subscribe<&gui::StatusBar::setFrameTimeAndFps>(statusBar.get());
    // statusBar->addControl(std::move(comboBox));
    // m_guiForm->add(std::move(statusBar));

    // m_guiForm->add(std::make_unique<gui::MemoryUsageBar>(m_guiForm.get()));
    // m_guiForm->processGuiUpdates();
    // m_guiForm->printGuiTree();

    m_sceneContainer = std::make_unique<SceneContainer>(m_renderer.get(), this, environment.getParameters().scene);
    m_sceneContainer->update(0.0f);

    gui::initImGui(
        m_window.getHandle(), *m_renderer, getIfExists<std::string>(environment.getConfig(), "imguiFontPath"));

    m_renderer->flushResourceUpdates(true);
}

Application::~Application()
{
    gui::shutdownImGui(*m_renderer);
}

void Application::run()
{
    Timer<std::chrono::duration<double>> updateTimer;
    double timeSinceLastUpdate = 0.0;
    while (!m_window.shouldClose())
    {
        const double timeDelta = updateTimer.restart();
        updateFrameStatistics(timeDelta);
        timeSinceLastUpdate += timeDelta;

        Window::pollEvents();
        if (m_isMinimized)
            continue;

        while (timeSinceLastUpdate > TimePerFrame)
        {
            m_sceneContainer->update(static_cast<float>(TimePerFrame));
            timeSinceLastUpdate -= TimePerFrame;
        }

        gui::prepareImGuiFrame();
        drawGui();
        m_sceneContainer->render();

        gui::renderImGuiFrame(*m_renderer);

        m_renderer->drawFrame();
    }

    m_renderer->finish();
}

void Application::close()
{
    m_window.close();
}

void Application::onResize(int width, int height)
{
    logger->info("New window dims: [{}, {}]", width, height);
    if (width == 0 || height == 0)
        return;

    m_renderer->resize(width, height);
    m_sceneContainer->resize(width, height);
}

SceneContainer* Application::getSceneContainer() const
{
    return m_sceneContainer.get();
}

gui::Form* Application::getForm() const
{
    return m_guiForm.get();
}

Window& Application::getWindow()
{
    return m_window;
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

void Application::drawGui()
{
    ImGui::Begin("Crisp");

    if (ImGui::BeginCombo("Scene", m_sceneContainer->getSceneName().c_str()))
    {
        for (const auto& scene : SceneContainer::getSceneNames())
        {
            const bool isSelected{scene == m_sceneContainer->getSceneName()};
            if (ImGui::Selectable(scene.c_str(), isSelected))
            {
                m_sceneContainer->onSceneSelected(scene);
            }
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}
} // namespace crisp
