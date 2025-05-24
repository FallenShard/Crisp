#include <Crisp/Core/Application.hpp>

#include <Crisp/Core/ChromeEventTracerIo.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/Profiler.hpp>
#include <Crisp/Core/Timer.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Renderer/AssetPaths.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_MT("Application");

[[maybe_unused]] void logFps(double frameTime, double fps) {
    CRISP_LOGD("{:03.2f} ms, {:03.2f} fps\r", frameTime, fps);
}

ImVec4 interpolateColor(const float t) {
    const glm::vec4 color =
        t > 0.5 ? glm::vec4{1.0f, 2.0f * (1.0f - t), 0.0f, 1.0f} : glm::vec4{t / 0.5f, 1.0f, 0.0f, 1.0f};
    return {color.r, color.g, color.b, color.a};
}

Window createWindow(const char* title, const glm::ivec2& size) {
    return {(Window::getDesktopResolution() - size) / 2, size, title};
}

AssetPaths createAssetPaths(const ApplicationEnvironment& environment) {
    return {
        .shaderSourceDir = environment.getShaderSourceDirectory(),
        .resourceDir = environment.getResourcesPath(),
        .spvShaderDir = environment.getResourcesPath() / "Shaders",
        .outputDir = environment.getOutputDirectory(),
    };
}
} // namespace

Application::Application(const ApplicationEnvironment& environment)
    : m_window(createWindow(kTitle, kDefaultWindowSize))
    , m_outputDir(environment.getOutputDirectory()) {
    const ScopeProfiler scope("Application constructor");

    VulkanCoreParams vulkanCoreParams{
        .requiredInstanceExtensions = ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
        .requiredDeviceExtensions = createDefaultDeviceExtensions(),
        .presentationMode = PresentationMode::DoubleBuffered,
    };
    addPageableMemoryDeviceExtensions(vulkanCoreParams.requiredDeviceExtensions);
    if (environment.getConfigParams().enableRayTracingExtension) {
        addRayTracingDeviceExtensions(vulkanCoreParams.requiredDeviceExtensions);
    }
    vulkanCoreParams.requiredDeviceExtensions.emplace_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    vulkanCoreParams.requiredDeviceExtensions.emplace_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);

    m_renderer = std::make_unique<Renderer>(
        std::move(vulkanCoreParams), m_window.createSurfaceCallback(), createAssetPaths(environment));

    m_window.resized.subscribe<&Application::onResize>(this);
    m_window.minimized.subscribe<&Application::onMinimize>(this);
    m_window.restored.subscribe<&Application::onRestore>(this);

    m_sceneContainer = std::make_unique<SceneContainer>(
        m_renderer.get(),
        &m_window,
        m_outputDir,
        environment.getConfigParams().scene,
        environment.getConfigParams().sceneArgs);
    m_sceneContainer->update({.frameIdx = 0, .frameInFlightIdx = 0, .dt = 0.0f, .totalTimeSec = 0.0f});

    gui::initImGui(
        m_window.getHandle(),
        m_renderer->getInstance().getHandle(),
        m_renderer->getPhysicalDevice().getHandle(),
        m_renderer->getDevice(),
        m_renderer->getSwapChain().getImageCount(),
        m_renderer->getDefaultRenderPass().getHandle(),
        environment.getConfigParams().imGuiFontPath);

    m_renderer->flushResourceUpdates(true);
}

Application::~Application() {
    gui::shutdownImGui(m_renderer->getDevice().getHandle());
    serializeTracedEvents(m_outputDir / "profiler.json");
}

void Application::run() {
    Timer<std::chrono::duration<double>> updateTimer;
    double timeSinceLastUpdate = 0.0;
    const auto beginTimePoint = std::chrono::steady_clock::now();
    while (!m_window.shouldClose()) {
        const double timeDelta = updateTimer.restart();
        updateFrameStatistics(timeDelta);
        timeSinceLastUpdate += timeDelta;

        const WindowEventGuard eventGuard(
            m_window, ImGui::GetIO().WantCaptureMouse ? EventType::AllMouseEvents : EventType::None);

        Window::pollEvents();
        resizeIfNeeded();

        while (timeSinceLastUpdate > kTimePerFrame) {
            m_sceneContainer->update({
                .frameIdx = static_cast<uint32_t>(m_renderer->getCurrentFrameIndex()),
                .frameInFlightIdx = m_renderer->getCurrentVirtualFrameIndex(),
                .dt = static_cast<float>(kTimePerFrame),
                .totalTimeSec = std::chrono::duration<float>(std::chrono::steady_clock::now() - beginTimePoint).count(),
            });
            timeSinceLastUpdate -= kTimePerFrame;
        }

        if (m_isMinimized) {
            continue;
        }

        gui::prepareImGui();
        drawGui();

        m_renderer->enqueueDefaultPassDrawCommand([](const VkCommandBuffer cmdBuffer) { gui::renderImGui(cmdBuffer); });

        const auto frameCtx{m_renderer->beginFrame()};
        if (!frameCtx) {
            continue;
        }

        m_sceneContainer->render(*frameCtx);
        m_renderer->record(*frameCtx);
        m_renderer->endFrame(*frameCtx);
    }

    m_renderer->finish();
}

void Application::close() {
    m_window.close();
}

void Application::onResize(int width, int height) {
    if (width == 0 || height == 0) {
        return;
    }

    m_isResizing = true;
}

Window& Application::getWindow() {
    return m_window;
}

void Application::updateFrameStatistics(double frameTime) {
    constexpr double kSecToMsec = 1000.0;

    m_accumulatedTime += frameTime;
    m_accumulatedFrames++;

    if (m_accumulatedTime >= m_updatePeriod) {
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

void Application::onMinimize() {
    m_isMinimized = true;
}

void Application::onRestore() {
    m_isMinimized = false;
}

void Application::resizeIfNeeded() {
    if (m_isResizing) {
        if (const auto size = m_window.getSize(); size.x != 0 && size.y != 0) {
            m_renderer->resize(size.x, size.y);
            m_sceneContainer->resize(size.x, size.y);
        }

        m_isResizing = false;
    }
}

void Application::drawGui() {
    ImGui::Begin("Application Settings");
    ImGui::LabelText("Frame", "%llu", m_renderer->getCurrentFrameIndex());           // NOLINT
    ImGui::LabelText("Frame Time", "%.2f ms, %.2f FPS", m_avgFrameTimeMs, m_avgFps); // NOLINT

    std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
    vmaGetHeapBudgets(m_renderer->getDevice().getMemoryAllocator(), budgets.data());

    const auto toMiB = [](const uint64_t bytes) {
        const uint64_t megaBytes = bytes >> 20;
        const uint64_t remainder = (bytes & ((1 << 20) - 1)) > 0; // NOLINT
        return megaBytes + remainder;
    };

    if (ImGui::CollapsingHeader("Memory Allocations")) {
        for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
            const auto& budget = budgets[i];
            if (budget.budget == 0) {
                continue;
            }

            const VkDeviceSize allocatedBytes = budget.statistics.blockBytes;
            const VkDeviceSize usedAllocatedBytes = budget.statistics.allocationBytes;

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                interpolateColor(static_cast<float>(usedAllocatedBytes) / static_cast<float>(allocatedBytes)));
            ImGui::LabelText( // NOLINT
                fmt::format("Heap {} Allocations", i).c_str(),
                "%llu / %llu MB",
                toMiB(usedAllocatedBytes),
                toMiB(allocatedBytes));
            ImGui::PopStyleColor();

            const VkDeviceSize totalAvailableBytes = budget.budget;
            const VkDeviceSize totalUsedBytes = budget.usage;

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                interpolateColor(static_cast<float>(totalUsedBytes) / static_cast<float>(totalAvailableBytes)));
            ImGui::LabelText( // NOLINT
                fmt::format("Heap {} Total", i).c_str(),
                "%llu / %llu MB",
                toMiB(totalUsedBytes),
                toMiB(totalAvailableBytes)); // NOLINT
            ImGui::PopStyleColor();
        }
    }

    gui::drawComboBox(
        "Scene",
        m_sceneContainer->getSceneName(),
        SceneContainer::getSceneNames(),
        [this](const std::string& selectedItem) { m_sceneContainer->onSceneSelected(selectedItem); });

    ImGui::End();
}
} // namespace crisp
