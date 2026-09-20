#include <Crisp/Vulkan/Benchmark/VulkanBenchmark.hpp>

#include <cmath>
#include <vector>

#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/ShaderUtils/Reflection.hpp>
#include <Crisp/Vulkan/PipelineBuilder.hpp>
#include <Crisp/Vulkan/PipelineLayoutBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRasterizationPassDescriptor.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {
namespace {
constexpr uint32_t kTargetWidth = 1920;
constexpr uint32_t kTargetHeight = 1080;
constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;

constexpr uint32_t kTargetTriangles = 2'000'000;

constexpr uint32_t kPatchVertexDim = 8;
constexpr uint32_t kPatchQuadDim = kPatchVertexDim - 1;
constexpr uint32_t kMeshletPrimitives = 2 * kPatchQuadDim * kPatchQuadDim;

struct GridPushConstants {
    float viewportWidth;
    float viewportHeight;
    float legPixels;
    uint32_t stridePerRow;
};

struct GridLayout {
    uint32_t quadsX;
    uint32_t quadsY;
    uint32_t trianglesPerDraw;
    uint32_t drawRepeats;

    uint64_t totalTriangles() const {
        return static_cast<uint64_t>(trianglesPerDraw) * drawRepeats;
    }
};

GridLayout computeGridLayout(const float legPixels) {
    GridLayout layout{};
    layout.quadsX = std::max(static_cast<uint32_t>(std::floor(kTargetWidth / legPixels)), 1u);
    layout.quadsY = std::max(static_cast<uint32_t>(std::floor(kTargetHeight / legPixels)), 1u);
    layout.trianglesPerDraw = 2 * layout.quadsX * layout.quadsY;
    layout.drawRepeats = std::max(kTargetTriangles / layout.trianglesPerDraw, 1u);
    return layout;
}

VkShaderModule createModule(const VulkanDevice& device, const std::filesystem::path& spvPath) {
    const auto code = readBinaryFile(spvPath).unwrap();
    VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // NOLINT
    VkShaderModule shaderModule{VK_NULL_HANDLE};
    VK_FATAL(vkCreateShaderModule(device.getHandle(), &moduleInfo, nullptr, &shaderModule));
    return shaderModule;
}

VkPipelineShaderStageCreateInfo createStage(const VkShaderModule module, const VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage = stage;
    info.module = module;
    info.pName = "main";
    return info;
}

class TrisFixture : public VulkanBenchmarkFixture {
protected:
    std::unique_ptr<VulkanPipeline> createRasterPipeline(
        const std::string_view firstShader, const VkShaderStageFlagBits firstStage) {
        auto& device = getDevice();

        const auto firstSpv = getBenchmarkShaderSpirv(firstShader);
        const auto fragSpv = getBenchmarkShaderSpirv("tris.frag.glsl");

        auto metadata = reflectPipelineLayoutFromSpirv(firstSpv).unwrap();
        metadata.merge(reflectPipelineLayoutFromSpirv(fragSpv).unwrap());
        PipelineLayoutBuilder layoutBuilder(std::move(metadata));
        auto layout = layoutBuilder.create(device);

        const VkShaderModule firstModule = createModule(device, firstSpv);
        const VkShaderModule fragModule = createModule(device, fragSpv);

        const VulkanRasterizationPassDescriptor passDescriptor{.colorAttachmentFormats = {kColorFormat}};

        PipelineBuilder builder{};
        builder.addShaderStage(createStage(firstModule, firstStage))
            .addShaderStage(createStage(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT))
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setCullMode(VK_CULL_MODE_NONE)
            .setDepthTest(VK_FALSE)
            .setDepthWrite(VK_FALSE)
            .setViewport(
                {0.0f, 0.0f, static_cast<float>(kTargetWidth), static_cast<float>(kTargetHeight), 0.0f, 1.0f})
            .setScissor({{0, 0}, {kTargetWidth, kTargetHeight}});
        auto pipeline = builder.create(device, std::move(layout), passDescriptor);

        vkDestroyShaderModule(device.getHandle(), firstModule, nullptr);
        vkDestroyShaderModule(device.getHandle(), fragModule, nullptr);
        return pipeline;
    }

    static VkRenderingAttachmentInfo makeAttachment(const VulkanImageView& view) {
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = view.getHandle();
        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        return attachment;
    }

    static VkRenderingInfo makeRenderingInfo(const VkRenderingAttachmentInfo& attachment) {
        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};
        info.renderArea = {{0, 0}, {kTargetWidth, kTargetHeight}};
        info.layerCount = 1;
        info.colorAttachmentCount = 1;
        info.pColorAttachments = &attachment;
        return info;
    }

    void reportRates(benchmark::State& state, const GridLayout& layout, const float legPixels) {
        setRateCounter(state, "triangles/s", static_cast<double>(layout.totalTriangles()));
        setRateCounter(
            state,
            "pixels/s",
            static_cast<double>(layout.totalTriangles()) * 0.5 * legPixels * legPixels);
        state.counters["legPx"] = benchmark::Counter(legPixels);
        state.counters["MTris"] = benchmark::Counter(static_cast<double>(layout.totalTriangles()) / 1.0e6);
    }

    std::unique_ptr<VulkanBuffer> createDeviceBuffer(
        const std::span<const uint32_t> data, const VkBufferUsageFlags2 usage) {
        auto& device = getDevice();
        const VkDeviceSize bytes = data.size_bytes();

        VulkanBuffer staging(device, bytes, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
        std::memcpy(staging.getHostVisibleData<uint32_t>(), data.data(), bytes);

        auto deviceBuffer = std::make_unique<VulkanBuffer>(
            device, bytes, usage | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::GpuOnly);
        device.getGeneralQueue().submitAndWait([&](const VkCommandBuffer cmdBuffer) {
            VulkanCommandEncoder{cmdBuffer}.copyBuffer(staging, *deviceBuffer);
        });
        return deviceBuffer;
    }
};
} // namespace

BENCHMARK_DEFINE_F(TrisFixture, NonIndexed)(benchmark::State& state) {
    const auto legPixels = static_cast<float>(state.range(0));
    const auto layout = computeGridLayout(legPixels);

    auto& device = getDevice();
    auto pipeline = createRasterPipeline("tris.vert.glsl", VK_SHADER_STAGE_VERTEX_BIT);

    VulkanImage colorImage(
        device,
        VkExtent3D{kTargetWidth, kTargetHeight, 1},
        1,
        1,
        kColorFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        0);
    auto colorView = createView(device, colorImage, VK_IMAGE_VIEW_TYPE_2D);

    const GridPushConstants pushConstants{
        .viewportWidth = static_cast<float>(kTargetWidth),
        .viewportHeight = static_cast<float>(kTargetHeight),
        .legPixels = legPixels,
        .stridePerRow = layout.quadsX,
    };

    bool transitioned = false;
    runTimedLoop(state, [&](const VulkanCommandEncoder& encoder) {
        if (!transitioned) {
            encoder.transitionLayout(
                colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, kNullStage >> kColorWrite);
            transitioned = true;
        }
        const auto attachment = makeAttachment(*colorView);
        encoder.beginRendering(makeRenderingInfo(attachment));
        encoder.bindPipeline(*pipeline);
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, pushConstants);
        for (uint32_t i = 0; i < layout.drawRepeats; ++i) {
            encoder.draw(layout.trianglesPerDraw * 3, 1, 0, 0);
        }
        encoder.endRendering();
    });

    reportRates(state, layout, legPixels);
    state.counters["vertsPerTri"] = benchmark::Counter(3.0);
}

BENCHMARK_DEFINE_F(TrisFixture, Indexed)(benchmark::State& state) {
    const auto legPixels = static_cast<float>(state.range(0));
    const auto layout = computeGridLayout(legPixels);

    auto& device = getDevice();
    auto pipeline = createRasterPipeline("tris-indexed.vert.glsl", VK_SHADER_STAGE_VERTEX_BIT);

    const uint32_t verticesPerRow = layout.quadsX + 1;
    const uint32_t vertexRows = layout.quadsY + 1;

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(layout.trianglesPerDraw) * 3);
    for (uint32_t y = 0; y < layout.quadsY; ++y) {
        for (uint32_t x = 0; x < layout.quadsX; ++x) {
            const uint32_t corner = x + y * verticesPerRow;
            indices.insert(indices.end(), {corner, corner + 1, corner + verticesPerRow});
            indices.insert(
                indices.end(), {corner + 1, corner + verticesPerRow + 1, corner + verticesPerRow});
        }
    }

    auto indexBuffer = createDeviceBuffer(indices, VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT);

    VulkanImage colorImage(
        device,
        VkExtent3D{kTargetWidth, kTargetHeight, 1},
        1,
        1,
        kColorFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        0);
    auto colorView = createView(device, colorImage, VK_IMAGE_VIEW_TYPE_2D);

    const GridPushConstants pushConstants{
        .viewportWidth = static_cast<float>(kTargetWidth),
        .viewportHeight = static_cast<float>(kTargetHeight),
        .legPixels = legPixels,
        .stridePerRow = verticesPerRow,
    };

    bool transitioned = false;
    runTimedLoop(state, [&](const VulkanCommandEncoder& encoder) {
        if (!transitioned) {
            encoder.transitionLayout(
                colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, kNullStage >> kColorWrite);
            transitioned = true;
        }
        const auto attachment = makeAttachment(*colorView);
        encoder.beginRendering(makeRenderingInfo(attachment));
        encoder.bindPipeline(*pipeline);
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, pushConstants);
        encoder.bindIndexBuffer(indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
        for (uint32_t i = 0; i < layout.drawRepeats; ++i) {
            encoder.drawIndexed(layout.trianglesPerDraw * 3, 1, 0, 0, 0);
        }
        encoder.endRendering();
    });

    reportRates(state, layout, legPixels);
    const double uniqueVertices = static_cast<double>(verticesPerRow) * vertexRows;
    state.counters["vertsPerTri"] = benchmark::Counter(uniqueVertices / layout.trianglesPerDraw);
}

BENCHMARK_DEFINE_F(TrisFixture, MeshShader)(benchmark::State& state) {
    const auto legPixels = static_cast<float>(state.range(0));
    const auto layout = computeGridLayout(legPixels);

    auto& device = getDevice();
    auto pipeline = createRasterPipeline("tris.mesh.glsl", VK_SHADER_STAGE_MESH_BIT_EXT);

    const uint32_t meshletsPerRow = (layout.quadsX + kPatchQuadDim - 1) / kPatchQuadDim;
    const uint32_t meshletRows = (layout.quadsY + kPatchQuadDim - 1) / kPatchQuadDim;
    const uint32_t meshletCount = meshletsPerRow * meshletRows;

    GridLayout meshLayout = layout;
    meshLayout.trianglesPerDraw = meshletCount * kMeshletPrimitives;
    meshLayout.drawRepeats = std::max(kTargetTriangles / meshLayout.trianglesPerDraw, 1u);

    VulkanImage colorImage(
        device,
        VkExtent3D{kTargetWidth, kTargetHeight, 1},
        1,
        1,
        kColorFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        0);
    auto colorView = createView(device, colorImage, VK_IMAGE_VIEW_TYPE_2D);

    const GridPushConstants pushConstants{
        .viewportWidth = static_cast<float>(kTargetWidth),
        .viewportHeight = static_cast<float>(kTargetHeight),
        .legPixels = legPixels,
        .stridePerRow = meshletsPerRow,
    };

    bool transitioned = false;
    runTimedLoop(state, [&](const VulkanCommandEncoder& encoder) {
        if (!transitioned) {
            encoder.transitionLayout(
                colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, kNullStage >> kColorWrite);
            transitioned = true;
        }
        const auto attachment = makeAttachment(*colorView);
        encoder.beginRendering(makeRenderingInfo(attachment));
        encoder.bindPipeline(*pipeline);
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_MESH_BIT_EXT, pushConstants);
        for (uint32_t i = 0; i < meshLayout.drawRepeats; ++i) {
            encoder.drawMeshTasks(meshletCount);
        }
        encoder.endRendering();
    });

    reportRates(state, meshLayout, legPixels);
    state.counters["vertsPerTri"] =
        benchmark::Counter(static_cast<double>(kPatchVertexDim * kPatchVertexDim) / kMeshletPrimitives);
    state.counters["meshlets"] = benchmark::Counter(static_cast<double>(meshletCount));
}

BENCHMARK_REGISTER_F(TrisFixture, NonIndexed)
    ->RangeMultiplier(2)
    ->Range(1, 64)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(TrisFixture, Indexed)
    ->RangeMultiplier(2)
    ->Range(1, 64)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(TrisFixture, MeshShader)
    ->RangeMultiplier(2)
    ->Range(1, 64)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);
} // namespace crisp

BENCHMARK_MAIN();
