#include <Crisp/Scenes/FluidSimulationScene.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

#include <gmock/gmock.h>

#include <cmath>
#include <vector>

namespace crisp {
namespace {

constexpr uint32_t kWidth = 640;
constexpr uint32_t kHeight = 360;

// At the default 16 substeps of 1/960 s this is half a second of simulated time: long enough for the
// column to slump, short enough to keep the test quick.
constexpr uint32_t kFrameCount = 30;
constexpr float kFixedDeltaTime = 1.0f / 60.0f;

class FluidSimulationSceneTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        spdlog::set_level(spdlog::level::warn);
        glfwInit();
    }

    static void TearDownTestSuite() {
        glfwTerminate();
    }
};

std::vector<glm::vec4> downloadPositions(Renderer& renderer, const VulkanBuffer& positions) {
    VulkanDevice& device = renderer.getDevice();
    VulkanBuffer readback(
        device, positions.getSize(), VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    device.getGeneralQueue().submitAndWait([&positions, &readback](VkCommandBuffer cmdBuffer) {
        const VulkanCommandEncoder encoder(cmdBuffer);
        encoder.copyBuffer(positions, readback);
        encoder.insertBufferMemoryBarrier(readback.createDescriptorInfo(), kTransferWrite >> kHostRead);
    });
    readback.invalidateMappedRange();

    const auto* mapped = readback.getHostVisibleData<glm::vec4>();
    return {mapped, mapped + positions.getSize() / sizeof(glm::vec4)}; // NOLINT
}

float meanHeight(const std::vector<glm::vec4>& positions) {
    double sum = 0.0;
    for (const auto& p : positions) {
        sum += p.y;
    }
    return static_cast<float>(sum / static_cast<double>(positions.size()));
}

TEST_F(FluidSimulationSceneTest, SolverAdvancesAndStaysBounded) {
    Window window(
        glm::ivec2{0, 0},
        glm::ivec2{static_cast<int32_t>(kWidth), static_cast<int32_t>(kHeight)},
        "fluid_simulation_test",
        WindowVisibility::Hidden);

    VulkanCoreParams coreParams{
        .requiredInstanceExtensions = ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
        .deviceFeatureRequests = createDefaultFeatureRequests(),
        .presentationMode = PresentationMode::DoubleBuffered,
        .includeValidation = true,
    };
    Renderer renderer(
        std::move(coreParams),
        window.createSurfaceCallback(),
        AssetPaths{
            .shaderSourceDir = CRISP_SHADER_SOURCE_DIR,
            .resourceDir = CRISP_RESOURCE_DIR,
            .spvShaderDir = std::filesystem::path{CRISP_RESOURCE_DIR} / "Shaders",
            .outputDir = std::filesystem::current_path(),
        });

    FluidSimulationScene scene(&renderer, &window);
    const SPH& sph = scene.getSimulation();

    renderer.finish();
    const auto initialPositions = downloadPositions(renderer, sph.getPositionBuffer());
    ASSERT_EQ(initialPositions.size(), sph.getParticleCount());
    const float initialMeanHeight = meanHeight(initialPositions);

    for (uint32_t frameIdx = 0; frameIdx < kFrameCount; ++frameIdx) {
        Window::pollEvents();
        scene.update({
            .frameIdx = frameIdx,
            .frameInFlightIdx = frameIdx % Renderer::NumVirtualFrames,
            .dt = kFixedDeltaTime,
            .totalTimeSec = static_cast<float>(frameIdx) * kFixedDeltaTime,
        });

        const auto frameContext = renderer.beginFrame();
        if (!frameContext) {
            continue;
        }
        scene.render(*frameContext);
        renderer.record(*frameContext);
        renderer.endFrame(*frameContext);
    }
    renderer.finish();

    const auto positions = downloadPositions(renderer, sph.getPositionBuffer());
    ASSERT_EQ(positions.size(), initialPositions.size());

    // The integrator clamps to the box, so anything outside it means the solver diverged before the
    // clamp saw it -- which is also how a NaN escapes.
    const glm::vec3 box = sph.getFluidSpaceSize();
    const float radius = sph.getParticleRadius();
    for (size_t i = 0; i < positions.size(); ++i) {
        const glm::vec3 p{positions[i]};
        ASSERT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)) << "particle " << i;
        EXPECT_THAT(p.x, ::testing::AllOf(::testing::Ge(0.0f), ::testing::Le(box.x)));
        EXPECT_THAT(p.y, ::testing::AllOf(::testing::Ge(0.0f), ::testing::Le(box.y)));
        EXPECT_THAT(p.z, ::testing::AllOf(::testing::Ge(0.0f), ::testing::Le(box.z)));
    }

    // Gravity is on and the column starts unsupported at the top, so it has to have slumped. This is
    // what fails if the substeps never dispatch or the graph orders them wrongly.
    EXPECT_LT(meanHeight(positions), initialMeanHeight - radius);
}

} // namespace
} // namespace crisp
