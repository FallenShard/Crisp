#include <Crisp/Core/Application.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/GUI/ImGuiUtils.hpp>
#include <Crisp/Utils/ChromeProfiler.hpp>

#include <imgui.h>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("Application");

[[maybe_unused]] void logFps(double frameTime, double fps)
{
    logger->debug("{:03.2f} ms, {:03.2f} fps\r", frameTime, fps);
}

ImVec4 interpolateColor(const float t)
{
    const glm::vec4 color =
        t > 0.5 ? glm::vec4{1.0f, 2.0f * (1.0f - t), 0.0f, 1.0f} : glm::vec4{t / 0.5f, 1.0f, 0.0f, 1.0f};
    return {color.r, color.g, color.b, color.a};
}

Window createWindow(const char* title, const glm::ivec2& size)
{
    return {(Window::getDesktopResolution() - size) / 2, size, title};
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
    : m_window(createWindow(kTitle, kDefaultWindowSize))
{
    m_renderer = std::make_unique<Renderer>(
        ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
        m_window.createSurfaceCallback(),
        createAssetPaths(environment),
        environment.getParameters().enableRayTracingExtension);
    logger->trace("Renderer created!");

    m_window.mouseButtonReleased.subscribe<&Application::onMouseButtonRelease>(this);
    m_window.resized.subscribe<&Application::onResize>(this);
    m_window.minimized.subscribe<&Application::onMinimize>(this);
    m_window.restored.subscribe<&Application::onRestore>(this);

    m_sceneContainer = std::make_unique<SceneContainer>(m_renderer.get(), &m_window, environment.getParameters().scene);
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

        WindowEventGuard eventGuard(
            m_window, ImGui::GetIO().WantCaptureMouse ? EventType::AllMouseEvents : EventType::None);

        Window::pollEvents();

        while (timeSinceLastUpdate > kTimePerFrame)
        {
            // m_sceneContainer->update(static_cast<float>(kTimePerFrame));
            timeSinceLastUpdate -= kTimePerFrame;
        }

        if (m_isMinimized)
        {
            continue;
        }

        gui::prepareImGuiFrame();
        drawGui();
        // m_sceneContainer->render();

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
    if (width == 0 || height == 0)
    {
        return;
    }

    m_isResizing = true;
}

SceneContainer* Application::getSceneContainer() const
{
    return m_sceneContainer.get();
}

gui::Form* Application::getForm() const // NOLINT
{
    return nullptr;
}

Window& Application::getWindow()
{
    return m_window;
}

void Application::updateFrameStatistics(double frameTime)
{
    constexpr double kSecToMsec = 1000.0;

    m_accumulatedTime += frameTime;
    m_accumulatedFrames++;

    if (m_accumulatedTime >= m_updatePeriod)
    {
        const double spillOver = m_accumulatedTime - m_updatePeriod;
        const double spillOverFrac = spillOver / frameTime;

        const double avgTime = m_updatePeriod / (m_accumulatedFrames - spillOverFrac);
        const double avgFrames = 1.0 / avgTime;

        onFrameTimeUpdated(avgTime, avgFrames);

        m_avgFrameTimeMs = avgTime * kSecToMsec;
        m_avgFps = avgFrames;

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

void Application::onMouseButtonRelease(const MouseEventArgs&)
{
    if (m_isResizing)
    {
        if (const auto size = m_window.getSize(); size.x != 0 && size.y != 0)
        {
            m_renderer->resize(size.x, size.y);
            m_sceneContainer->resize(size.x, size.y);
        }

        m_isResizing = false;
    }
}

void Application::drawGui()
{
    ImGui::Begin("Application Settings");
    ImGui::LabelText("Frame Time", "%.2f ms, %.2f FPS", m_avgFrameTimeMs, m_avgFps); // NOLINT

    const auto metrics = m_renderer->getDevice().getMemoryAllocator().getDeviceMemoryUsage();

    const auto drawMemoryLabel = [](const char* label, const uint64_t bytesUsed, const uint64_t bytesAllocated)
    {
        const auto toMegabytes = [](const uint64_t bytes)
        {
            const uint64_t megaBytes = bytes >> 20;
            const uint64_t remainder = (bytes & ((1 << 20) - 1)) > 0; // NOLINT
            return megaBytes + remainder;
        };
        ImGui::PushStyleColor(
            ImGuiCol_Text, interpolateColor(static_cast<float>(bytesUsed) / static_cast<float>(bytesAllocated)));
        ImGui::LabelText(label, "%llu / %llu MB", toMegabytes(bytesUsed), toMegabytes(bytesAllocated)); // NOLINT
        ImGui::PopStyleColor();
    };

    drawMemoryLabel("Buffer Memory", metrics.bufferMemoryUsed, metrics.bufferMemorySize);
    drawMemoryLabel("Image Memory", metrics.imageMemoryUsed, metrics.imageMemorySize);
    drawMemoryLabel("Staging Memory", metrics.stagingMemoryUsed, metrics.stagingMemorySize);

    // gui::drawComboBox(
    //     "Scene",
    //     m_sceneContainer->getSceneName(),
    //     SceneContainer::getSceneNames(),
    //     [this](const std::string& selectedItem) { m_sceneContainer->onSceneSelected(selectedItem); });

    ImGui::End();
}
} // namespace crisp
