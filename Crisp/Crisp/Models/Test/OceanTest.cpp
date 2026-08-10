#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

#include <numeric>

namespace crisp {
namespace {
using OceanTest = VulkanTest;

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

// TEST_F(OceanTest, PatchConstruction)
//{
//     constexpr int32_t N = 512;
//     constexpr float kSize = 5.0;
//     const std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {
//         {VertexAttribute::Position}, {VertexAttribute::Normal}};
//
//     TriangleMesh mesh = createGridMesh(flatten(vertexFormat), kSize, N - 1);
//     EXPECT_EQ(mesh.getVertexCount(), N * N);
//
//     const auto boundingBox = mesh.getBoundingBox();
//     EXPECT_NEAR(boundingBox.getExtents().x, kSize, 1e-8);
//     EXPECT_NEAR(boundingBox.getExtents().y, 0.0, 1e-8);
//     EXPECT_NEAR(boundingBox.getExtents().z, kSize, 1e-8);
// }

TEST_F(OceanTest, VulkanBuffer) {
    constexpr VkDeviceSize elementCount = 25;
    constexpr VkDeviceSize size = sizeof(float) * elementCount;
    std::vector<float> data(elementCount);
    std::iota(data.begin(), data.end(), 0.0f);

    VulkanBuffer deviceBuffer(
        *device_,
        size,
        VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT,
        BufferMemoryType::GpuOnly);

    VulkanBuffer stagingBuffer(*device_, size, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(stagingPtr[i], data[i]);
    }

    VulkanBuffer downloadBuffer(
        *device_, deviceBuffer.getSize(), VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);
    {
        ScopeCommandExecutor executor(*device_);
        const auto& cmdEncoder = executor.cmdEncoder;

        cmdEncoder.copyBuffer(stagingBuffer, deviceBuffer);
        cmdEncoder.insertBufferMemoryBarrier(
            deviceBuffer.createDescriptorInfo(), kTransferWrite >> kTransferRead);

        cmdEncoder.copyBuffer(deviceBuffer, downloadBuffer);
        cmdEncoder.insertBufferMemoryBarrier(
            downloadBuffer.createDescriptorInfo(), kTransferWrite >> kHostRead);
    }

    const float* ptr = downloadBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < elementCount; ++i) {
        ASSERT_EQ(ptr[i], data[i]) << " not equal at index " << i;
    }
}

constexpr int32_t kFftGridSize = 16;
constexpr int32_t kFftLogGridSize = 4;

std::filesystem::path testShaderSpv(const std::string& fileName) {
    return std::filesystem::path{"TestData"} / "CrispOceanTest" / (fileName + ".spv");
}

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

// Binds a fresh descriptor set from a fresh pipeline instance, mirroring how OceanScene.cpp wires
// one pipeline+material pair per FFT pass rather than sharing one pipeline's descriptor pool across
// several sets (createComputePipeline() sizes the pool for a single allocation).
FftDispatch createImageToImageDispatch(
    const VulkanDevice& device,
    const std::filesystem::path& spv,
    const VkExtent3D& workGroupSize,
    const VulkanImageView& srcView,
    const VulkanImageView& dstView) {
    FftDispatch dispatch{};
    dispatch.pipeline = createComputePipeline(device, spv, workGroupSize);
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

struct BitReversalPushConstants {
    int32_t reversalDirection; // 0 is horizontal, 1 is vertical.
    int32_t passCount;
};

struct IfftPushConstants {
    int32_t passIdx;
    int32_t N;
};

// A Hermitian-symmetric spectrum (h(-k) = conj(h(k))) must inverse-transform to a real-valued
// signal. This runs the actual ocean-spectrum/bit-reverse/ifft compute shaders end to end on a
// small grid and checks that the final normalZ image's imaginary channel is ~0 -- the invariant
// the Hermitian-symmetry fix in ocean-spectrum.comp.glsl relies on, and the free correctness
// assertion the ocean-scene plan calls for ahead of the shared-memory FFT rewrite. normalZ (rather
// than the height/dispX channel) is checked because it's the one FFT channel ocean-spectrum.comp
// leaves unpaired -- height and dispX are packed into one complex FFT (IFFT(A + i*B) = a(x) +
// i*b(x)), so that channel's imaginary component is meaningful spatial data (dispX), not residue.
TEST_F(OceanTest, InverseTransformOfHermitianSpectrumIsReal) {
    VulkanDevice& device = *device_;
    const VkExtent3D workGroupSize{static_cast<uint32_t>(kFftGridSize), static_cast<uint32_t>(kFftGridSize), 1};
    const VkExtent3D workGroupCount = computeWorkGroupCount(glm::uvec3(kFftGridSize, kFftGridSize, 1), workGroupSize);

    // smallWaves (l) is set to 4x cell size, matching kCellSize scaling used in production
    // (OceanScene.cpp): it damps energy near the Nyquist frequency, where a non-axis-aligned wind
    // direction otherwise breaks the spectrum's Hermitian symmetry (see the -k negation in
    // calculatePhillipsSpectrum() aliasing incorrectly at the Nyquist row/column).
    const OceanParameters params = createOceanParameters(
        kFftGridSize, /*patchWorldSize=*/16.0f, /*windX=*/10.0f, /*windZ=*/3.0f, /*A=*/0.01f, /*l=*/4.0f);
    const auto seedSpectrum = createOceanSpectrum(/*seed=*/7, params);

    auto seedImage = createFftImage(device, kFftGridSize);
    auto packedHeightDispX = createFftImage(device, kFftGridSize);
    auto packedDispZNormalX = createFftImage(device, kFftGridSize);
    auto normalZ = createFftImage(device, kFftGridSize);
    auto bitRevH = createFftImage(device, kFftGridSize);
    auto bitRevV = createFftImage(device, kFftGridSize);
    std::vector<std::unique_ptr<VulkanImage>> ifftH;
    std::vector<std::unique_ptr<VulkanImage>> ifftV;
    for (int32_t i = 0; i < kFftLogGridSize; ++i) {
        ifftH.push_back(createFftImage(device, kFftGridSize));
        ifftV.push_back(createFftImage(device, kFftGridSize));
    }

    auto seedView = createView(device, *seedImage, VK_IMAGE_VIEW_TYPE_2D);
    auto packedHeightDispXView = createView(device, *packedHeightDispX, VK_IMAGE_VIEW_TYPE_2D);
    auto packedDispZNormalXView = createView(device, *packedDispZNormalX, VK_IMAGE_VIEW_TYPE_2D);
    auto normalZView = createView(device, *normalZ, VK_IMAGE_VIEW_TYPE_2D);
    auto bitRevHView = createView(device, *bitRevH, VK_IMAGE_VIEW_TYPE_2D);
    auto bitRevVView = createView(device, *bitRevV, VK_IMAGE_VIEW_TYPE_2D);
    std::vector<std::unique_ptr<VulkanImageView>> ifftHViews;
    std::vector<std::unique_ptr<VulkanImageView>> ifftVViews;
    for (int32_t i = 0; i < kFftLogGridSize; ++i) {
        ifftHViews.push_back(createView(device, *ifftH[i], VK_IMAGE_VIEW_TYPE_2D));
        ifftVViews.push_back(createView(device, *ifftV[i], VK_IMAGE_VIEW_TYPE_2D));
    }

    auto spectrumPipeline = createComputePipeline(device, testShaderSpv("ocean-spectrum.comp.glsl"), workGroupSize);
    Material spectrumMaterial(spectrumPipeline.get());
    spectrumMaterial.writeDescriptor(0, 0, seedView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(0, 1, packedHeightDispXView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(
        0, 2, packedDispZNormalXView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    spectrumMaterial.writeDescriptor(0, 3, normalZView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    const auto bitRevSpv = testShaderSpv("ocean-reverse-bits.comp.glsl");
    const auto ifftHoriSpv = testShaderSpv("ifft-hori.comp.glsl");
    const auto ifftVertSpv = testShaderSpv("ifft-vert.comp.glsl");

    FftDispatch bitRevHDispatch =
        createImageToImageDispatch(device, bitRevSpv, workGroupSize, *normalZView, *bitRevHView);
    std::vector<FftDispatch> ifftHDispatches;
    for (int32_t i = 0; i < kFftLogGridSize; ++i) {
        const auto& srcView = i == 0 ? *bitRevHView : *ifftHViews[i - 1];
        ifftHDispatches.push_back(
            createImageToImageDispatch(device, ifftHoriSpv, workGroupSize, srcView, *ifftHViews[i]));
    }
    FftDispatch bitRevVDispatch = createImageToImageDispatch(
        device, bitRevSpv, workGroupSize, *ifftHViews[kFftLogGridSize - 1], *bitRevVView);
    std::vector<FftDispatch> ifftVDispatches;
    for (int32_t i = 0; i < kFftLogGridSize; ++i) {
        const auto& srcView = i == 0 ? *bitRevVView : *ifftVViews[i - 1];
        ifftVDispatches.push_back(
            createImageToImageDispatch(device, ifftVertSpv, workGroupSize, srcView, *ifftVViews[i]));
    }

    device.flushDescriptorUpdates();

    VulkanImage& finalImage = *ifftV[kFftLogGridSize - 1];
    VulkanBuffer downloadBuffer(
        device,
        static_cast<VkDeviceSize>(kFftGridSize) * kFftGridSize * sizeof(glm::vec2),
        VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        BufferMemoryType::HostReadback);

    {
        const ScopeCommandExecutor executor(device);
        const auto& encoder = executor.cmdEncoder;

        const VkDeviceSize seedByteSize = seedSpectrum.size() * sizeof(seedSpectrum[0]);
        VulkanBuffer seedStaging(
            device, seedByteSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
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
             {packedHeightDispX.get(), packedDispZNormalX.get(), normalZ.get(), bitRevH.get(), bitRevV.get()}) {
            encoder.transitionLayout(*image, VK_IMAGE_LAYOUT_GENERAL, kNullStage >> kComputeStorageWrite);
        }
        for (int32_t i = 0; i < kFftLogGridSize; ++i) {
            encoder.transitionLayout(*ifftH[i], VK_IMAGE_LAYOUT_GENERAL, kNullStage >> kComputeStorageWrite);
            encoder.transitionLayout(*ifftV[i], VK_IMAGE_LAYOUT_GENERAL, kNullStage >> kComputeStorageWrite);
        }

        encoder.bindPipeline(*spectrumPipeline);
        encoder.bindDescriptorSets(spectrumMaterial.getDescriptorSetBinding());
        encoder.setPushConstants(*spectrumPipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, params);
        encoder.dispatchCompute(workGroupCount);
        encoder.insertBarrier(kComputeStorageWrite >> kComputeStorageRead);

        runFftDispatch(encoder, bitRevHDispatch, BitReversalPushConstants{0, kFftLogGridSize}, workGroupCount);
        for (int32_t i = 0; i < kFftLogGridSize; ++i) {
            runFftDispatch(encoder, ifftHDispatches[i], IfftPushConstants{i + 1, kFftGridSize}, workGroupCount);
        }
        runFftDispatch(encoder, bitRevVDispatch, BitReversalPushConstants{1, kFftLogGridSize}, workGroupCount);
        for (int32_t i = 0; i < kFftLogGridSize; ++i) {
            runFftDispatch(encoder, ifftVDispatches[i], IfftPushConstants{i + 1, kFftGridSize}, workGroupCount);
        }

        encoder.transitionLayout(
            finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kComputeStorageWrite >> kTransferRead);
        const VkBufferImageCopy finalRegion{
            .imageSubresource =
                {.aspectMask = finalImage.getAspectMask(), .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .imageExtent = {finalImage.getWidth(), finalImage.getHeight(), 1},
        };
        encoder.copyImageToBuffer(finalImage, downloadBuffer, finalRegion);
    }

    // Checked relative to the signal's own scale (driven by the wind/amplitude parameters above)
    // rather than an absolute epsilon: float32 butterfly arithmetic accumulates roundoff
    // proportional to the magnitude of the values being transformed, not to a fixed constant.
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
