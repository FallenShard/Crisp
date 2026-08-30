#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Scenes/EnvironmentLightSampling.hpp>
#include <Crisp/Scenes/RayTracingSceneParser.hpp>
#include <Crisp/ShaderUtils/Test/TestShaderMap.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

#include <array>
#include <cmath>
#include <numbers>

namespace crisp {
namespace {

using EnvironmentSamplingTest = VulkanTest;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 4;
constexpr uint32_t kSampleCount = 1u << 16;
const auto kShaderSourceDirectory = std::filesystem::path{"TestData"} / "CrispEnvironmentSamplingTest";
const TestShaderMap kTestShaders{kShaderSourceDirectory / "environment-sampling.comp.glsl"};

struct SamplingResult {
    glm::vec4 directionAndPdf;
    glm::uvec4 texel;
};

struct PushConstants {
    VkDeviceAddress distribution;
    uint32_t width;
    uint32_t height;
    uint32_t sampleCount;
    uint32_t pad0;
};

static_assert(sizeof(PushConstants) == 24);
static_assert(sizeof(SamplingResult) == 32);

std::vector<float> createTestPixels() {
    std::vector<float> pixels(kWidth * kHeight * 4, 1.0f);
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const float value = 0.25f + static_cast<float>(1 + x + 2 * y);
            const size_t offset = (static_cast<size_t>(y) * kWidth + x) * 4;
            pixels[offset + 0] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
        }
    }
    return pixels;
}

std::vector<SamplingResult> runSamplingShader(
    VulkanDevice& device, const Distribution2D& distribution) {
    const VkDeviceSize distributionByteSize = distribution.getCdf().size() * sizeof(float);
    VulkanBuffer distributionBuffer(
        device, distributionByteSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, BufferMemoryType::HostUpload);
    distributionBuffer.updateFromHost(distribution.getCdf());

    const VkDeviceSize resultByteSize = kSampleCount * sizeof(SamplingResult);
    VulkanBuffer resultBuffer(
        device,
        resultByteSize,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
        BufferMemoryType::GpuOnly);
    VulkanBuffer readbackBuffer(
        device, resultByteSize, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    constexpr VkExtent3D kWorkGroupSize{64, 1, 1};
    auto pipeline =
        createComputePipeline(device, kTestShaders.getSpirvPath("environment-sampling.comp.glsl"), kWorkGroupSize);
    Material material(pipeline.get());
    material.writeDescriptor(0, 0, resultBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    {
        const ScopeCommandExecutor executor(device);
        const auto& encoder = executor.cmdEncoder;
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(
            *pipeline->getPipelineLayout(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            PushConstants{
                distributionBuffer.getDeviceAddress(),
                distribution.getColumnCount(),
                distribution.getRowCount(),
                kSampleCount,
                0,
            });
        encoder.dispatchCompute(computeWorkGroupCount(glm::uvec3{kSampleCount, 1, 1}, kWorkGroupSize));
        encoder.insertBufferMemoryBarrier(resultBuffer.createDescriptorInfo(), kComputeStorageWrite >> kTransferRead);
        encoder.copyBuffer(resultBuffer, readbackBuffer);
        encoder.insertBufferMemoryBarrier(readbackBuffer.createDescriptorInfo(), kTransferWrite >> kHostRead);
    }

    const auto* data = readbackBuffer.getHostVisibleData<SamplingResult>();
    return {data, data + kSampleCount}; // NOLINT
}

TEST(EnvironmentSamplingDistributionTest, BuildsNormalizedLuminanceTimesSineDistribution) {
    const auto pixels = createTestPixels();
    const auto distribution = createEnvironmentSamplingDistribution(pixels, kWidth, kHeight);

    double probabilitySum = 0.0;
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const float probability = distribution.getCellProbability(x, y);
            EXPECT_GE(probability, 0.0f);
            probabilitySum += probability;
        }
    }
    EXPECT_NEAR(probabilitySum, 1.0, 2e-6);

    const float northProbability = distribution.getCellProbability(0, 0);
    const float equatorialProbability = distribution.getCellProbability(0, 1);
    const float luminanceRatio = 3.25f / 1.25f;
    const float sineRatio =
        std::sin(1.5f * std::numbers::pi_v<float> / kHeight) /
        std::sin(0.5f * std::numbers::pi_v<float> / kHeight);
    EXPECT_NEAR(equatorialProbability / northProbability, luminanceRatio * sineRatio, 2e-5f);
}

TEST_F(EnvironmentSamplingTest, SamplesMatchCdfAndReportSolidAnglePdf) {
    const auto pixels = createTestPixels();
    const auto distribution = createEnvironmentSamplingDistribution(pixels, kWidth, kHeight);
    const auto results = runSamplingShader(*device_, distribution);
    std::array<uint32_t, kWidth * kHeight> observed{};

    for (const auto& result : results) {
        ASSERT_LT(result.texel.x, kWidth);
        ASSERT_LT(result.texel.y, kHeight);
        ++observed[result.texel.y * kWidth + result.texel.x];

        const glm::vec3 direction = result.directionAndPdf;
        ASSERT_NEAR(glm::dot(direction, direction), 1.0f, 2e-5f);
        ASSERT_TRUE(std::isfinite(result.directionAndPdf.w));
        ASSERT_GT(result.directionAndPdf.w, 0.0f);

        const float probability = distribution.getCellProbability(result.texel.x, result.texel.y);
        const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - direction.y * direction.y));
        const float expectedPdf = probability * float(kWidth * kHeight) /
            (2.0f * std::numbers::pi_v<float> * std::numbers::pi_v<float> * sinTheta);
        EXPECT_NEAR(result.directionAndPdf.w, expectedPdf, 2e-5f * expectedPdf + 1e-7f);
    }

    double chiSquared = 0.0;
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const double expected =
                kSampleCount * static_cast<double>(distribution.getCellProbability(x, y));
            const double difference = observed[y * kWidth + x] - expected;
            chiSquared += difference * difference / expected;
        }
    }
    // 31 degrees of freedom; this is above the 99.99th percentile while still catching systematic CDF errors.
    EXPECT_LT(chiSquared, 70.0);
}

TEST(EnvironmentLightParserTest, ParsesEnvironmentFilenameAndScale) {
    const nlohmann::json lights = nlohmann::json::array({
        {{"type", "environment"}, {"filename", "Textures/studio.hdr"}, {"radianceScale", 2.5f}},
    });
    const auto scene = parseSceneDescription(nlohmann::json::array(), lights);
    ASSERT_TRUE(scene.environment.has_value());
    EXPECT_EQ(scene.environment->filename, "Textures/studio.hdr");
    EXPECT_FLOAT_EQ(scene.environment->radianceScale, 2.5f);
}

TEST(EnvironmentLightParserTest, RejectsInvalidOrDuplicateEnvironments) {
    const nlohmann::json missingFilename = nlohmann::json::array({{{"type", "environment"}}});
    EXPECT_THROW(parseSceneDescription(nlohmann::json::array(), missingFilename), std::invalid_argument);

    const nlohmann::json negativeScale = nlohmann::json::array({
        {{"type", "environment"}, {"filename", "studio.hdr"}, {"radianceScale", -1.0f}},
    });
    EXPECT_THROW(parseSceneDescription(nlohmann::json::array(), negativeScale), std::invalid_argument);

    const nlohmann::json duplicate = nlohmann::json::array({
        {{"type", "environment"}, {"filename", "studio.hdr"}},
        {{"type", "environment"}, {"filename", "studio.hdr"}},
    });
    EXPECT_THROW(parseSceneDescription(nlohmann::json::array(), duplicate), std::invalid_argument);
}

} // namespace
} // namespace crisp
