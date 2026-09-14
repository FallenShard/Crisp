#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/ShaderUtils/Test/TestShaderMap.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace crisp {
namespace {
using OceanTest = VulkanTest;

const auto kShaderSourceDirectory = std::filesystem::path{"TestData"} / "CrispOceanTest";
const TestShaderMap kTestShaders{
    kShaderSourceDirectory / "Ocean" / "spectrum.comp.glsl",
    kShaderSourceDirectory / "Ocean" / "ifft.comp.glsl",
};

TEST(OceanClipmapTest, RingsTileTheLevelBelow) {
    const OceanClipmap clipmap{};

    // Level 0's outer half-extent has to equal level 1's inner one, or the rings gap. Both sides are
    // written in blocks, so this is really a check that the shader's 4x4-minus-2x2 layout is the one
    // the sizes were derived from.
    const float blockQuads = static_cast<float>(clipmap.blockQuads);
    for (int32_t level = 1; level < clipmap.levelCount; ++level) {
        const float finerSpacing = clipmap.finestSpacing * std::exp2(static_cast<float>(level - 1));
        const float coarserSpacing = 2.0f * finerSpacing;
        EXPECT_FLOAT_EQ(2.0f * blockQuads * finerSpacing, blockQuads * coarserSpacing);
    }

    EXPECT_EQ(
        computeClipmapInstanceCount(clipmap),
        kOceanClipmapLevel0Blocks + kOceanClipmapRingBlocks * (clipmap.levelCount - 1));
    // Reaching the geometric horizon from any plausible eye height is the whole point.
    EXPECT_GT(computeClipmapRadius(clipmap), 100.0f * 1000.0f);
}

TEST(OceanClipmapTest, OriginStaysOnTheSnapGrid) {
    const OceanClipmap clipmap{};

    for (const float x : {-1234.5f, -0.5f, 0.0f, 7.25f, 5000.125f}) {
        const glm::vec2 origin = computeClipmapOrigin(clipmap, glm::vec2(x, -x));
        // Off the grid, levels stop being aligned to their own spacing and their vertices swim.
        EXPECT_FLOAT_EQ(std::fmod(origin.x, clipmap.snapGrid), 0.0f);
        EXPECT_FLOAT_EQ(std::fmod(origin.y, clipmap.snapGrid), 0.0f);
        // And it has to actually follow the camera, within one cell.
        EXPECT_LT(std::abs(origin.x - x), clipmap.snapGrid);
        EXPECT_LT(std::abs(origin.y + x), clipmap.snapGrid);
    }
}

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
    createInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
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

// Every transform is packed now, so "the inverse transform is real" is no longer checkable: each one
// carries a real field in its imaginary half by design. What is checkable, and is the property the
// spectral Jacobian rests on, is that dDx/dx transformed from the spectrum agrees with a finite
// difference of the transformed Dx. A packing slip, a wrong sign, or a broken Hermitian pair all
// show up here as disagreement. See docs/ocean.md items 9a and 18.
TEST_F(OceanTest, SpectralJacobianMatchesFiniteDifferencedDisplacement) {
    VulkanDevice& device = *device_;
    const VkExtent3D workGroupSize{static_cast<uint32_t>(kFftGridSize), static_cast<uint32_t>(kFftGridSize), 1};
    const VkExtent3D workGroupCount = computeWorkGroupCount(glm::uvec3(kFftGridSize, kFftGridSize, 1), workGroupSize);

    constexpr float kPatchWorldSize = 16.0f;
    // smallWaves damps energy near Nyquist, where a central difference stops approximating the exact
    // derivative well enough to compare against.
    const OceanParameters params =
        createOceanParameters(kFftGridSize, /*windX=*/10.0f, /*windZ=*/3.0f, /*A=*/0.01f, /*l=*/4.0f);
    // A single band spanning the whole grid: the derivative identity is under test, not banding.
    const OceanCascade cascade{
        .patchWorldSize = kPatchWorldSize,
        .kMin = 0.0f,
        .kMax = std::numeric_limits<float>::max(),
    };
    const auto seedSpectrum = createOceanSpectrum(/*seed=*/7, params, /*cascadeCount=*/1);

    auto seedImage = createFftImage(device, kFftGridSize);
    auto packedDisplacement = createFftImage(device, kFftGridSize);
    auto packedJacobian = createFftImage(device, kFftGridSize);

    auto seedView = createView(device, *seedImage, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    auto packedDisplacementView = createView(device, *packedDisplacement, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    auto packedJacobianView = createView(device, *packedJacobian, VK_IMAGE_VIEW_TYPE_2D_ARRAY);

    auto spectrumPipeline =
        createComputePipeline(device, kTestShaders.getSpirvPath("spectrum.comp.glsl"), workGroupSize);
    Material spectrumMaterial(spectrumPipeline.get());
    spectrumMaterial.writeDescriptor(0, 0, seedView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(0, 1, packedDisplacementView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(0, 2, packedJacobianView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    // One workgroup per line, N/2 threads each, matching how OceanScene dispatches these.
    constexpr VkExtent3D kIfftWorkGroupSize{kFftGridSize / 2, 1, 1};
    const SpecializationConstantMap kHoriConstants{{kMaxNConstantId, kFftGridSize}};
    const SpecializationConstantMap kVertConstants{
        {kMaxNConstantId, kFftGridSize}, {kApplyOriginShiftConstantId, VK_TRUE}, {kTransposedConstantId, VK_TRUE}};
    const auto& ifftSpv = kTestShaders.getSpirvPath("ifft.comp.glsl");

    // One scratch pair and one dispatch pair per transform under test; the materials bake their
    // source and destination, so the two chains cannot share scratch.
    struct IfftChain {
        std::unique_ptr<VulkanImage> hori;
        std::unique_ptr<VulkanImage> vert;
        std::unique_ptr<VulkanImageView> horiView;
        std::unique_ptr<VulkanImageView> vertView;
        FftDispatch horiDispatch;
        FftDispatch vertDispatch;
        std::unique_ptr<VulkanBuffer> download;
    };

    const auto makeChain = [&](const VulkanImageView& source) {
        IfftChain chain{};
        chain.hori = createFftImage(device, kFftGridSize);
        chain.vert = createFftImage(device, kFftGridSize);
        chain.horiView = createView(device, *chain.hori, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        chain.vertView = createView(device, *chain.vert, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        chain.horiDispatch =
            createImageToImageDispatch(device, ifftSpv, kIfftWorkGroupSize, source, *chain.horiView, kHoriConstants);
        chain.vertDispatch = createImageToImageDispatch(
            device, ifftSpv, kIfftWorkGroupSize, *chain.horiView, *chain.vertView, kVertConstants);
        chain.download = std::make_unique<VulkanBuffer>(
            device,
            static_cast<VkDeviceSize>(kFftGridSize) * kFftGridSize * sizeof(glm::vec4),
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
            BufferMemoryType::HostReadback);
        return chain;
    };

    // After the transform, displacement is (height, dispX, dispZ, slopeX) and jacobian is
    // (slopeZ, dDx/dx, dDz/dz, dDx/dz), so both fields under test land in the .g channel.
    IfftChain displacementChain = makeChain(*packedDisplacementView);
    IfftChain jacobianChain = makeChain(*packedJacobianView);

    device.flushDescriptorUpdates();

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
             {packedDisplacement.get(),
              packedJacobian.get(),
              displacementChain.hori.get(),
              displacementChain.vert.get(),
              jacobianChain.hori.get(),
              jacobianChain.vert.get()}) {
            encoder.transitionLayout(*image, VK_IMAGE_LAYOUT_GENERAL, kNullStage >> kComputeStorageWrite);
        }

        encoder.bindPipeline(*spectrumPipeline);
        encoder.bindDescriptorSets(spectrumMaterial.getDescriptorSetBinding());
        encoder.setPushConstants(
            *spectrumPipeline->getPipelineLayout(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            createOceanSpectrumPushConstants(params, cascade, /*cascadeIndex=*/0));
        encoder.dispatchCompute(workGroupCount);
        encoder.insertBarrier(kComputeStorageWrite >> kComputeStorageRead);

        constexpr IfftPushConstants kIfftPushConstants{kFftGridSize, kFftLogGridSize};
        constexpr VkExtent3D kIfftDispatchSize{1, kFftGridSize, 1};
        for (IfftChain* chain : {&displacementChain, &jacobianChain}) {
            runFftDispatch(encoder, chain->horiDispatch, kIfftPushConstants, kIfftDispatchSize);
            runFftDispatch(encoder, chain->vertDispatch, kIfftPushConstants, kIfftDispatchSize);

            encoder.transitionLayout(
                *chain->vert, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kComputeStorageWrite >> kTransferRead);
            const VkBufferImageCopy region{
                .imageSubresource =
                    {.aspectMask = chain->vert->getAspectMask(), .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                .imageExtent = {chain->vert->getWidth(), chain->vert->getHeight(), 1},
            };
            encoder.copyImageToBuffer(*chain->vert, *chain->download, region);
        }
    }

    constexpr size_t kTexelCount = static_cast<size_t>(kFftGridSize) * kFftGridSize;
    const std::span<const glm::vec4> displacement(
        displacementChain.download->getHostVisibleData<glm::vec4>(), kTexelCount);
    const std::span<const glm::vec4> jacobian(jacobianChain.download->getHostVisibleData<glm::vec4>(), kTexelCount);

    constexpr float kCellSize = kPatchWorldSize / kFftGridSize;
    double squaredError = 0.0;
    double squaredReference = 0.0;
    for (int32_t y = 0; y < kFftGridSize; ++y) {
        for (int32_t x = 0; x < kFftGridSize; ++x) {
            // The field is periodic, so the central difference wraps at the edges.
            const int32_t left = (x - 1 + kFftGridSize) % kFftGridSize;
            const int32_t right = (x + 1) % kFftGridSize;
            const float dispLeft = displacement[static_cast<size_t>(y) * kFftGridSize + left].y;   // NOLINT
            const float dispRight = displacement[static_cast<size_t>(y) * kFftGridSize + right].y; // NOLINT

            const float finiteDifference = (dispRight - dispLeft) / (2.0f * kCellSize);
            const float spectral = jacobian[static_cast<size_t>(y) * kFftGridSize + x].y; // NOLINT

            const double diff = static_cast<double>(finiteDifference) - static_cast<double>(spectral);
            squaredError += diff * diff;
            squaredReference += static_cast<double>(spectral) * static_cast<double>(spectral);
        }
    }

    ASSERT_GT(squaredReference, 0.0);
    // Central differences carry O(h^2) truncation error, so this is an agreement bound rather than
    // equality. A sign flip or a packing slip lands far above it.
    EXPECT_LT(std::sqrt(squaredError / squaredReference), 0.15);
}

} // namespace
} // namespace crisp
