#include <Crisp/Models/Pbf.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

#include <gmock/gmock.h>

#include <cmath>
#include <vector>

namespace crisp {
namespace {

constexpr uint32_t kWidth = 320;
constexpr uint32_t kHeight = 200;

constexpr uint32_t kFrameCount = 120;
constexpr float kFixedDeltaTime = 1.0f / 60.0f;

constexpr float kRestDensity = 1000.0f;

class PbfTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        spdlog::set_level(spdlog::level::warn);
        glfwInit();
    }

    static void TearDownTestSuite() {
        glfwTerminate();
    }
};

template <typename T>
std::vector<T> download(Renderer& renderer, const VulkanBuffer& buffer) {
    VulkanDevice& device = renderer.getDevice();
    VulkanBuffer readback(device, buffer.getSize(), VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    device.getGeneralQueue().submitAndWait([&buffer, &readback](VkCommandBuffer cmdBuffer) {
        const VulkanCommandEncoder encoder(cmdBuffer);
        encoder.copyBuffer(buffer, readback);
        encoder.insertBufferMemoryBarrier(readback.createDescriptorInfo(), kTransferWrite >> kHostRead);
    });
    readback.invalidateMappedRange();

    const auto* mapped = readback.getHostVisibleData<T>();
    return {mapped, mapped + buffer.getSize() / sizeof(T)}; // NOLINT
}

float meanHeight(const std::vector<glm::vec4>& positions) {
    double sum = 0.0;
    for (const auto& p : positions) {
        sum += p.y;
    }
    return static_cast<float>(sum / static_cast<double>(positions.size()));
}

struct Harness {
    Window window{
        glm::ivec2{0, 0},
        glm::ivec2{static_cast<int32_t>(kWidth), static_cast<int32_t>(kHeight)},
        "pbf_test",
        WindowVisibility::Hidden};

    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<PositionBasedFluid> fluid;
    std::unique_ptr<rg::RenderGraph> renderGraph;

    Harness() {
        VulkanCoreParams coreParams{
            .requiredInstanceExtensions = ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
            .deviceFeatureRequests = createDefaultFeatureRequests(),
            .presentationMode = PresentationMode::DoubleBuffered,
            .includeValidation = true,
        };
        renderer = std::make_unique<Renderer>(
            std::move(coreParams),
            window.createSurfaceCallback(),
            AssetPaths{
                .shaderSourceDir = CRISP_SHADER_SOURCE_DIR,
                .resourceDir = CRISP_RESOURCE_DIR,
                .spvShaderDir = std::filesystem::path{CRISP_RESOURCE_DIR} / "Shaders",
                .outputDir = std::filesystem::current_path(),
            });

        fluid = std::make_unique<PositionBasedFluid>(*renderer, PbfConfig{});
        renderGraph = std::make_unique<rg::RenderGraph>();
        fluid->addComputePasses(*renderGraph);
        renderGraph->compile(renderer->getDevice(), renderer->getSwapChainExtent());
    }

    void step(const uint32_t /*frameIdx*/) const {
        Window::pollEvents();
        fluid->update(kFixedDeltaTime);

        const auto frameContext = renderer->beginFrame();
        if (!frameContext) {
            return;
        }
        renderGraph->execute(*frameContext);
        renderer->record(*frameContext);
        renderer->endFrame(*frameContext);
    }
};

TEST_F(PbfTest, ColumnCollapsesAndStaysIncompressible) {
    Harness h;
    const PositionBasedFluid& fluid = *h.fluid;

    h.renderer->finish();
    const auto initialPositions = download<glm::vec4>(*h.renderer, fluid.getPositionBuffer());
    ASSERT_EQ(initialPositions.size(), fluid.getParticleCount());
    const float initialMeanHeight = meanHeight(initialPositions);

    for (uint32_t frameIdx = 0; frameIdx < kFrameCount; ++frameIdx) {
        h.step(frameIdx);
    }
    h.renderer->finish();

    const auto positions = download<glm::vec4>(*h.renderer, fluid.getPositionBuffer());
    ASSERT_EQ(positions.size(), initialPositions.size());

    const glm::vec3 box = fluid.getFluidSpaceSize();
    for (size_t i = 0; i < positions.size(); ++i) {
        const glm::vec3 p{positions[i]};
        ASSERT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)) << "particle " << i;
        EXPECT_THAT(p.x, ::testing::AllOf(::testing::Ge(0.0f), ::testing::Le(box.x)));
        EXPECT_THAT(p.y, ::testing::AllOf(::testing::Ge(0.0f), ::testing::Le(box.y)));
        EXPECT_THAT(p.z, ::testing::AllOf(::testing::Ge(0.0f), ::testing::Le(box.z)));
    }

    EXPECT_LT(meanHeight(positions), 0.85f * initialMeanHeight);
    const auto densities = download<float>(*h.renderer, fluid.getDensityBuffer());
    ASSERT_EQ(densities.size(), positions.size());
    double sum = 0.0;
    float peak = 0.0f;
    for (const float density : densities) {
        ASSERT_TRUE(std::isfinite(density));
        sum += density;
        peak = std::max(peak, density);
    }
    const auto mean = static_cast<float>(sum / static_cast<double>(densities.size()));
    EXPECT_THAT(mean, ::testing::AllOf(::testing::Gt(0.5f * kRestDensity), ::testing::Lt(1.3f * kRestDensity)));
    EXPECT_LT(peak, 2.5f * kRestDensity);
}

} // namespace
} // namespace crisp
