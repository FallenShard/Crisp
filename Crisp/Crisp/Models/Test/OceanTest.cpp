#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/ShaderUtils/Test/TestShaderMap.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

#include <array>
#include <numeric>

namespace crisp {
namespace {
using OceanTest = VulkanTest;

const auto kShaderSourceDirectory = std::filesystem::path{"TestData"} / "CrispOceanTest";
const TestShaderMap kTestShaders{
    kShaderSourceDirectory / "ocean-spectrum.comp.glsl",
    kShaderSourceDirectory / "ifft.comp.glsl",
};

TEST_F(OceanTest, PatchConstruction) {
    constexpr float kSize = 5.0;
    for (const int32_t N : {64, 128, 256}) {
        const TriangleMesh mesh = createGridMesh(kSize, N - 1);
        EXPECT_EQ(mesh.getVertexCount(), N * N);

        const auto boundingBox = mesh.getBoundingBox();
        EXPECT_NEAR(boundingBox.getExtents().x, kSize, 1e-8);
        EXPECT_NEAR(boundingBox.getExtents().y, 0.0, 1e-8);
        EXPECT_NEAR(boundingBox.getExtents().z, kSize, 1e-8);
    }
}

TEST_F(OceanTest, VulkanBuffer) {
    constexpr VkDeviceSize elementCount = 25;
    constexpr VkDeviceSize size = sizeof(float) * elementCount;
    std::vector<float> data(elementCount);
    std::iota(data.begin(), data.end(), 0.0f); // NOLINT

    VulkanBuffer deviceBuffer(
        *device_,
        size,
        VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT,
        BufferMemoryType::GpuOnly);

    VulkanBuffer stagingBuffer(*device_, size, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(stagingPtr[i], data[i]); // NOLINT
    }

    VulkanBuffer downloadBuffer(
        *device_, deviceBuffer.getSize(), VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);
    {
        ScopeCommandExecutor executor(*device_);
        const auto& cmdEncoder = executor.cmdEncoder;

        cmdEncoder.copyBuffer(stagingBuffer, deviceBuffer);
        cmdEncoder.insertBufferMemoryBarrier(deviceBuffer.createDescriptorInfo(), kTransferWrite >> kTransferRead);

        cmdEncoder.copyBuffer(deviceBuffer, downloadBuffer);
        cmdEncoder.insertBufferMemoryBarrier(downloadBuffer.createDescriptorInfo(), kTransferWrite >> kHostRead);
    }

    const float* ptr = downloadBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < elementCount; ++i) {
        ASSERT_EQ(ptr[i], data[i]) << " not equal at index " << i; // NOLINT
    }
}

constexpr int32_t kFftGridSize = 16;
constexpr int32_t kFftLogGridSize = 4;

std::unique_ptr<VulkanImage> createFftImage(const VulkanDevice& device, const uint32_t size) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.arrayLayers = 1;
    createInfo.extent = {size, size, 1};
    createInfo.format = VK_FORMAT_R32G32_SFLOAT;
    createInfo.mipLevels = 1;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    return std::make_unique<VulkanImage>(device, createInfo);
}

struct FftDispatch {
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<Material> material;
};

// One pipeline+material pair per pass: createComputePipeline() sizes the pool for a single set.
FftDispatch createImageToImageDispatch(
    const VulkanDevice& device,
    const std::filesystem::path& spv,
    const VkExtent3D& workGroupSize,
    const VulkanImageView& srcView,
    const VulkanImageView& dstView,
    const SpecializationConstantMap& specializationConstants = {}) {
    FftDispatch dispatch{};
    dispatch.pipeline = createComputePipeline(device, spv, workGroupSize, {}, specializationConstants);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    dispatch.material->writeDescriptor(0, 0, srcView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(0, 1, dstView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

template <typename PushConstants>
void runFftDispatch(
    const VulkanCommandEncoder& encoder,
    const FftDispatch& dispatch,
    const PushConstants& pushConstants,
    const VkExtent3D& workGroupCount) {
    encoder.bindPipeline(*dispatch.pipeline);
    encoder.bindDescriptorSets(dispatch.material->getDescriptorSetBinding());
    encoder.setPushConstants(*dispatch.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pushConstants);
    encoder.dispatchCompute(workGroupCount);
    encoder.insertBarrier(kComputeStorageWrite >> kComputeStorageRead);
}

struct IfftPushConstants {
    int32_t N;
    int32_t logN;
};

// Must match the constant_id declarations in ifft.comp.glsl; 0..2 are the work group dimensions.
constexpr uint32_t kMaxNConstantId = 3;
constexpr uint32_t kApplyOriginShiftConstantId = 4;
constexpr uint32_t kTransposedConstantId = 5;

// A Hermitian spectrum must inverse-transform to a real signal. normalZ is checked because it is
// the one unpaired channel: the packed ones carry real spatial data in their imaginary half.
TEST_F(OceanTest, InverseTransformOfHermitianSpectrumIsReal) {
    VulkanDevice& device = *device_;
    const VkExtent3D workGroupSize{static_cast<uint32_t>(kFftGridSize), static_cast<uint32_t>(kFftGridSize), 1};
    const VkExtent3D workGroupCount = computeWorkGroupCount(glm::uvec3(kFftGridSize, kFftGridSize, 1), workGroupSize);

    // smallWaves damps energy near Nyquist, where an off-axis wind otherwise breaks Hermitian symmetry.
    const OceanParameters params = createOceanParameters(
        kFftGridSize, /*patchWorldSize=*/16.0f, /*windX=*/10.0f, /*windZ=*/3.0f, /*A=*/0.01f, /*l=*/4.0f);
    const auto seedSpectrum = createOceanSpectrum(/*seed=*/7, params);

    auto seedImage = createFftImage(device, kFftGridSize);
    auto packedHeightDispX = createFftImage(device, kFftGridSize);
    auto packedDispZNormalX = createFftImage(device, kFftGridSize);
    auto normalZ = createFftImage(device, kFftGridSize);
    auto ifftHori = createFftImage(device, kFftGridSize);
    auto ifftVert = createFftImage(device, kFftGridSize);

    auto seedView = createView(device, *seedImage, VK_IMAGE_VIEW_TYPE_2D);
    auto packedHeightDispXView = createView(device, *packedHeightDispX, VK_IMAGE_VIEW_TYPE_2D);
    auto packedDispZNormalXView = createView(device, *packedDispZNormalX, VK_IMAGE_VIEW_TYPE_2D);
    auto normalZView = createView(device, *normalZ, VK_IMAGE_VIEW_TYPE_2D);
    auto ifftHoriView = createView(device, *ifftHori, VK_IMAGE_VIEW_TYPE_2D);
    auto ifftVertView = createView(device, *ifftVert, VK_IMAGE_VIEW_TYPE_2D);

    auto spectrumPipeline =
        createComputePipeline(device, kTestShaders.getSpirvPath("ocean-spectrum.comp.glsl"), workGroupSize);
    Material spectrumMaterial(spectrumPipeline.get());
    spectrumMaterial.writeDescriptor(0, 0, seedView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(0, 1, packedHeightDispXView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(0, 2, packedDispZNormalXView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(0, 3, normalZView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    // One workgroup per line, N/2 threads each, matching how OceanScene dispatches these.
    constexpr VkExtent3D kIfftWorkGroupSize{kFftGridSize / 2, 1, 1};
    const SpecializationConstantMap kHoriConstants{{kMaxNConstantId, kFftGridSize}};
    const SpecializationConstantMap kVertConstants{
        {kMaxNConstantId, kFftGridSize},
        {kApplyOriginShiftConstantId, VK_TRUE},
        {kTransposedConstantId, VK_TRUE}};

    const auto& ifftSpv = kTestShaders.getSpirvPath("ifft.comp.glsl");
    FftDispatch ifftHoriDispatch = createImageToImageDispatch(
        device, ifftSpv, kIfftWorkGroupSize, *normalZView, *ifftHoriView, kHoriConstants);
    FftDispatch ifftVertDispatch = createImageToImageDispatch(
        device, ifftSpv, kIfftWorkGroupSize, *ifftHoriView, *ifftVertView, kVertConstants);

    device.flushDescriptorUpdates();

    VulkanImage& finalImage = *ifftVert;
    VulkanBuffer downloadBuffer(
        device,
        static_cast<VkDeviceSize>(kFftGridSize) * kFftGridSize * sizeof(glm::vec2),
        VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        BufferMemoryType::HostReadback);

    {
        const ScopeCommandExecutor executor(device);
        const auto& encoder = executor.cmdEncoder;

        const VkDeviceSize seedByteSize = seedSpectrum.size() * sizeof(seedSpectrum[0]);
        VulkanBuffer seedStaging(device, seedByteSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
        seedStaging.updateFromHost(seedSpectrum);

        const VkBufferImageCopy seedRegion{
            .imageSubresource =
                {.aspectMask = seedImage->getAspectMask(), .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .imageExtent = {seedImage->getWidth(), seedImage->getHeight(), 1},
        };
        encoder.transitionLayout(*seedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
        encoder.copyBufferToImage(seedStaging, *seedImage, seedRegion);
        encoder.transitionLayout(*seedImage, VK_IMAGE_LAYOUT_GENERAL, kTransferWrite >> kComputeStorageRead);

        for (VulkanImage* image :
             {packedHeightDispX.get(), packedDispZNormalX.get(), normalZ.get(), ifftHori.get(), ifftVert.get()}) {
            encoder.transitionLayout(*image, VK_IMAGE_LAYOUT_GENERAL, kNullStage >> kComputeStorageWrite);
        }

        encoder.bindPipeline(*spectrumPipeline);
        encoder.bindDescriptorSets(spectrumMaterial.getDescriptorSetBinding());
        encoder.setPushConstants(*spectrumPipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, params);
        encoder.dispatchCompute(workGroupCount);
        encoder.insertBarrier(kComputeStorageWrite >> kComputeStorageRead);

        constexpr IfftPushConstants kIfftPushConstants{kFftGridSize, kFftLogGridSize};
        constexpr VkExtent3D kIfftDispatchSize{1, kFftGridSize, 1};
        runFftDispatch(encoder, ifftHoriDispatch, kIfftPushConstants, kIfftDispatchSize);
        runFftDispatch(encoder, ifftVertDispatch, kIfftPushConstants, kIfftDispatchSize);

        encoder.transitionLayout(
            finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kComputeStorageWrite >> kTransferRead);
        const VkBufferImageCopy finalRegion{
            .imageSubresource =
                {.aspectMask = finalImage.getAspectMask(), .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .imageExtent = {finalImage.getWidth(), finalImage.getHeight(), 1},
        };
        encoder.copyImageToBuffer(finalImage, downloadBuffer, finalRegion);
    }

    // Relative: float32 butterfly roundoff scales with the magnitude being transformed.
    const std::span<const glm::vec2> result(
        downloadBuffer.getHostVisibleData<glm::vec2>(), static_cast<size_t>(kFftGridSize) * kFftGridSize);
    float maxAbsReal = 0.0f;
    float maxAbsImag = 0.0f;
    for (const glm::vec2& texel : result) {
        maxAbsReal = std::max(maxAbsReal, std::abs(texel.x));
        maxAbsImag = std::max(maxAbsImag, std::abs(texel.y));
    }
    ASSERT_GT(maxAbsReal, 0.0f);
    EXPECT_LT(maxAbsImag / maxAbsReal, 1e-2f);
}

} // namespace
} // namespace crisp
