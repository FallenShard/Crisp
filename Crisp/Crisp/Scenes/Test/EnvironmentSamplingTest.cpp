#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Scenes/EnvironmentLightSampling.hpp>
#include <Crisp/Scenes/RayTracingSceneParser.hpp>
#include <Crisp/ShaderUtils/Test/TestShaderMap.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

#include <array>
#include <cmath>
#include <numbers>

#include <Crisp/Core/Test/ResultTestUtils.hpp>

namespace crisp {
namespace {

using EnvironmentSamplingTest = VulkanTest;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 4;
constexpr uint32_t kSampleCount = 1u << 16;
const auto kShaderSourceDirectory = std::filesystem::path{CRISP_TEST_ASSET_DIR};
const TestShaderMap kTestShaders{
    {kShaderSourceDirectory / "environment-sampling.comp.glsl",
     kShaderSourceDirectory / "point-light.comp.glsl",
     kShaderSourceDirectory / "directional-light.comp.glsl"},
    std::filesystem::path{CRISP_SHADER_SOURCE_DIR}};

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

struct PointLightResult {
    glm::vec4 directionAndDistance;
    glm::vec4 radianceAndPdf;
};

struct PointLightPushConstants {
    glm::vec3 position;
    float pad0{};
    glm::vec3 power;
    float pad1{};
    glm::vec3 reference;
    float pad2{};
};

static_assert(sizeof(PointLightResult) == 32);
static_assert(sizeof(PointLightPushConstants) == 48);

struct DirectionalLightResult {
    glm::vec4 directionAndDistance;
    glm::vec4 irradianceAndPdf;
};

struct DirectionalLightPushConstants {
    glm::vec3 direction;
    float pad0{};
    glm::vec3 irradiance;
    float pad1{};
};

static_assert(sizeof(DirectionalLightResult) == 32);
static_assert(sizeof(DirectionalLightPushConstants) == 32);

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

std::vector<SamplingResult> runSamplingShader(VulkanDevice& device, const Distribution2D& distribution) {
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

std::array<PointLightResult, 2> runPointLightShader(VulkanDevice& device, const PointLightPushConstants& pushConstants) {
    constexpr VkDeviceSize kResultByteSize = 2 * sizeof(PointLightResult);
    VulkanBuffer resultBuffer(
        device,
        kResultByteSize,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
        BufferMemoryType::GpuOnly);
    VulkanBuffer readbackBuffer(
        device, kResultByteSize, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    constexpr VkExtent3D kWorkGroupSize{1, 1, 1};
    auto pipeline = createComputePipeline(device, kTestShaders.getSpirvPath("point-light.comp.glsl"), kWorkGroupSize);
    Material material(pipeline.get());
    material.writeDescriptor(0, 0, resultBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    {
        const ScopeCommandExecutor executor(device);
        const auto& encoder = executor.cmdEncoder;
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pushConstants);
        encoder.dispatchCompute({1, 1, 1});
        encoder.insertBufferMemoryBarrier(resultBuffer.createDescriptorInfo(), kComputeStorageWrite >> kTransferRead);
        encoder.copyBuffer(resultBuffer, readbackBuffer);
        encoder.insertBufferMemoryBarrier(readbackBuffer.createDescriptorInfo(), kTransferWrite >> kHostRead);
    }

    const auto* data = readbackBuffer.getHostVisibleData<PointLightResult>();
    return {data[0], data[1]};
}

DirectionalLightResult runDirectionalLightShader(
    VulkanDevice& device, const DirectionalLightPushConstants& pushConstants) {
    constexpr VkDeviceSize kResultByteSize = sizeof(DirectionalLightResult);
    VulkanBuffer resultBuffer(
        device,
        kResultByteSize,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
        BufferMemoryType::GpuOnly);
    VulkanBuffer readbackBuffer(
        device, kResultByteSize, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    constexpr VkExtent3D kWorkGroupSize{1, 1, 1};
    auto pipeline =
        createComputePipeline(device, kTestShaders.getSpirvPath("directional-light.comp.glsl"), kWorkGroupSize);
    Material material(pipeline.get());
    material.writeDescriptor(0, 0, resultBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    {
        const ScopeCommandExecutor executor(device);
        const auto& encoder = executor.cmdEncoder;
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pushConstants);
        encoder.dispatchCompute({1, 1, 1});
        encoder.insertBufferMemoryBarrier(resultBuffer.createDescriptorInfo(), kComputeStorageWrite >> kTransferRead);
        encoder.copyBuffer(resultBuffer, readbackBuffer);
        encoder.insertBufferMemoryBarrier(readbackBuffer.createDescriptorInfo(), kTransferWrite >> kHostRead);
    }

    return *readbackBuffer.getHostVisibleData<DirectionalLightResult>();
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
        std::sin(1.5f * std::numbers::pi_v<float> / kHeight) / std::sin(0.5f * std::numbers::pi_v<float> / kHeight);
    EXPECT_NEAR(equatorialProbability / northProbability, luminanceRatio * sineRatio, 2e-5f);
}

TEST(EnvironmentSamplingDistributionTest, SupportsSinglePixelWhiteFurnace) {
    constexpr std::array<float, 4> kUnitRadiance{1.0f, 1.0f, 1.0f, 1.0f};
    const auto distribution = createEnvironmentSamplingDistribution(kUnitRadiance, 1, 1);

    EXPECT_EQ(distribution.getColumnCount(), 1);
    EXPECT_EQ(distribution.getRowCount(), 1);
    EXPECT_DOUBLE_EQ(distribution.getWeightSum(), 1.0);
    EXPECT_FLOAT_EQ(distribution.getCellProbability(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(distribution.getPdf(glm::vec2(0.5f)), 1.0f);
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
        const float expectedPdf =
            probability * float(kWidth * kHeight) /
            (2.0f * std::numbers::pi_v<float> * std::numbers::pi_v<float> * sinTheta);
        EXPECT_NEAR(result.directionAndPdf.w, expectedPdf, 2e-5f * expectedPdf + 1e-7f);
    }

    double chiSquared = 0.0;
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const double expected = kSampleCount * static_cast<double>(distribution.getCellProbability(x, y));
            const double difference = observed[y * kWidth + x] - expected;
            chiSquared += difference * difference / expected;
        }
    }
    // 31 degrees of freedom; this is above the 99.99th percentile while still catching systematic CDF errors.
    EXPECT_LT(chiSquared, 70.0);
}

TEST_F(EnvironmentSamplingTest, PointLightUsesPowerAndInverseSquareFalloff) {
    const PointLightPushConstants pushConstants{
        .position = {1.0f, 2.0f, 3.0f},
        .power = {20.0f, 40.0f, 60.0f},
        .reference = {-1.0f, 2.0f, -1.0f},
    };
    const auto results = runPointLightShader(*device_, pushConstants);

    const glm::vec3 lightVector = pushConstants.position - pushConstants.reference;
    const float squaredDistance = glm::dot(lightVector, lightVector);
    const glm::vec3 expectedRadiance = pushConstants.power / (4.0f * std::numbers::pi_v<float> * squaredDistance);

    EXPECT_NEAR(results[0].directionAndDistance.x, glm::normalize(lightVector).x, 1e-6f);
    EXPECT_NEAR(results[0].directionAndDistance.y, glm::normalize(lightVector).y, 1e-6f);
    EXPECT_NEAR(results[0].directionAndDistance.z, glm::normalize(lightVector).z, 1e-6f);
    EXPECT_NEAR(results[0].directionAndDistance.w, std::sqrt(squaredDistance), 1e-6f);
    EXPECT_NEAR(results[0].radianceAndPdf.x, expectedRadiance.x, 1e-6f);
    EXPECT_NEAR(results[0].radianceAndPdf.y, expectedRadiance.y, 1e-6f);
    EXPECT_NEAR(results[0].radianceAndPdf.z, expectedRadiance.z, 1e-6f);
    EXPECT_FLOAT_EQ(results[0].radianceAndPdf.w, 1.0f);

    EXPECT_EQ(results[1].directionAndDistance, glm::vec4(0.0f));
    EXPECT_EQ(results[1].radianceAndPdf, glm::vec4(0.0f));
}

TEST_F(EnvironmentSamplingTest, DirectionalLightUsesConstantIrradianceAndInfiniteShadowRay) {
    const DirectionalLightPushConstants pushConstants{
        .direction = glm::normalize(glm::vec3(3.0f, -4.0f, -12.0f)),
        .irradiance = {1.0f, 2.0f, 3.0f},
    };
    const auto result = runDirectionalLightShader(*device_, pushConstants);

    EXPECT_NEAR(result.directionAndDistance.x, -pushConstants.direction.x, 1e-6f);
    EXPECT_NEAR(result.directionAndDistance.y, -pushConstants.direction.y, 1e-6f);
    EXPECT_NEAR(result.directionAndDistance.z, -pushConstants.direction.z, 1e-6f);
    EXPECT_GT(result.directionAndDistance.w, 1e20f);
    EXPECT_EQ(glm::vec3(result.irradianceAndPdf), pushConstants.irradiance);
    EXPECT_FLOAT_EQ(result.irradianceAndPdf.w, 1.0f);
}

TEST(EnvironmentLightParserTest, ParsesEnvironmentFilenameAndScale) {
    const nlohmann::json lights = nlohmann::json::array({
        {{"type", "environment"}, {"filename", "Textures/studio.hdr"}, {"radianceScale", 2.5f}},
    });
    const auto result = parseSceneDescription(nlohmann::json::array(), lights);
    ASSERT_TRUE(result.hasValue());
    const auto& scene = *result;
    ASSERT_TRUE(scene.environment.has_value());
    ASSERT_TRUE(scene.environment->filename.has_value());
    EXPECT_EQ(*scene.environment->filename, "Textures/studio.hdr");
    EXPECT_FALSE(scene.environment->radiance.has_value());
    EXPECT_FLOAT_EQ(scene.environment->radianceScale, 2.5f);
}

TEST(EnvironmentLightParserTest, ParsesWhiteFurnaceRadiance) {
    const nlohmann::json lights = nlohmann::json::array({
        {{"type", "environment"}, {"radiance", {1.0f, 1.0f, 1.0f}}},
    });
    const auto result = parseSceneDescription(nlohmann::json::array(), lights);
    ASSERT_TRUE(result.hasValue());
    const auto& scene = *result;

    ASSERT_TRUE(scene.environment.has_value());
    EXPECT_FALSE(scene.environment->filename.has_value());
    ASSERT_TRUE(scene.environment->radiance.has_value());
    EXPECT_EQ(*scene.environment->radiance, glm::vec3(1.0f));
    EXPECT_FLOAT_EQ(scene.environment->radianceScale, 1.0f);
}

TEST(EnvironmentLightParserTest, RejectsInvalidOrDuplicateEnvironments) {
    const nlohmann::json missingFilename = nlohmann::json::array({{{"type", "environment"}}});
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), missingFilename),
        HasErrorWithMessageRegex("exactly one of filename or radiance"));

    const nlohmann::json negativeScale = nlohmann::json::array({
        {{"type", "environment"}, {"filename", "studio.hdr"}, {"radianceScale", -1.0f}},
    });
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), negativeScale),
        HasErrorWithMessageRegex("radianceScale must be finite and non-negative"));

    const nlohmann::json negativeRadiance = nlohmann::json::array({
        {{"type", "environment"}, {"radiance", {1.0f, -1.0f, 1.0f}}},
    });
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), negativeRadiance),
        HasErrorWithMessageRegex("radiance must be non-negative"));

    const nlohmann::json ambiguousEnvironment = nlohmann::json::array({
        {{"type", "environment"}, {"filename", "studio.hdr"}, {"radiance", {1.0f, 1.0f, 1.0f}}},
    });
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), ambiguousEnvironment),
        HasErrorWithMessageRegex("exactly one of filename or radiance"));

    const nlohmann::json duplicate = nlohmann::json::array({
        {{"type", "environment"}, {"filename", "studio.hdr"}},
        {{"type", "environment"}, {"filename", "studio.hdr"}},
    });
    EXPECT_THAT(
        parseSceneDescription(nlohmann::json::array(), duplicate),
        HasErrorWithMessageRegex("Only one environment light"));
}

} // namespace
} // namespace crisp
