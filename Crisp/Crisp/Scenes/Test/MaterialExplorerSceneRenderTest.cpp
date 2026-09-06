#include <Crisp/Scenes/MaterialExplorerScene.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

#include <gmock/gmock.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace crisp {
namespace {

constexpr uint32_t kWidth = 640;
constexpr uint32_t kHeight = 360;

// The camera damps toward its configured orientation; accumulation restarts on every frame it still moves, so
// the image only converges once it has settled.
constexpr uint32_t kWarmupFrames = 24;
constexpr uint32_t kResizeFrame = 3;
constexpr float kFixedDeltaTime = 1.0f / 60.0f;

// A path-traced image is a Monte Carlo estimate, so this is a scale comparison, not a bit comparison. The
// sampler is seeded per pixel and per accumulated sample, so a same-device rerun is nevertheless deterministic.
constexpr float kMaxRelativeRmse = 0.02f;

std::filesystem::path goldenImagePath() {
    return std::filesystem::path{"TestData"} / "CrispMaterialExplorerSceneTest" / "material-explorer-path-traced.exr";
}

class MaterialExplorerSceneRenderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        spdlog::set_level(spdlog::level::warn);
        glfwInit();
    }

    static void TearDownTestSuite() {
        glfwTerminate();
    }
};

std::vector<float> renderPathTracedView(bool& supported) {
    Window window(
        glm::ivec2{0, 0},
        glm::ivec2{static_cast<int32_t>(kWidth), static_cast<int32_t>(kHeight)},
        "material_explorer_render_test",
        WindowVisibility::Hidden);

    VulkanCoreParams coreParams{
        .requiredInstanceExtensions = ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
        .deviceFeatureRequests = createDefaultFeatureRequests(),
        .presentationMode = PresentationMode::DoubleBuffered,
        .includeValidation = true,
    };
    addPageableMemoryFeatures(coreParams.deviceFeatureRequests);
    addMeshShadingFeatures(coreParams.deviceFeatureRequests);
    addRayTracingFeatures(coreParams.deviceFeatureRequests);
    addRayQueryFeatures(coreParams.deviceFeatureRequests);
    addDescriptorHeapFeatures(coreParams.deviceFeatureRequests);
    addShaderUntypedPointersFeatures(coreParams.deviceFeatureRequests);
    Renderer renderer(
        std::move(coreParams),
        window.createSurfaceCallback(),
        AssetPaths{
            .shaderSourceDir = CRISP_SHADER_SOURCE_DIR,
            .resourceDir = CRISP_RESOURCE_DIR,
            .spvShaderDir = std::filesystem::path{CRISP_RESOURCE_DIR} / "Shaders",
            .outputDir = std::filesystem::current_path(),
        });

    const auto& features = renderer.getDevice().getEnabledFeatures();
    supported = features.rayTracing && features.descriptorHeap && features.shaderUntypedPointers;
    if (!supported) {
        return {};
    }

    const nlohmann::json args{
        {"renderMode", "path-traced"},
        {"modelPath", "Models/sphere.obj"},
        {"environmentMap", "NewportLoft"},
    };
    MaterialExplorerScene scene(&renderer, &window, args);

    for (uint32_t frameIdx = 0; frameIdx < kWarmupFrames; ++frameIdx) {
        // Recompiling reallocates every physical image, so anything holding a graph view has to rebind it.
        if (frameIdx == kResizeFrame) {
            renderer.finish();
            scene.resize(static_cast<int32_t>(kWidth), static_cast<int32_t>(kHeight));
        }

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
        // Without this nothing transitions the swapchain image out of UNDEFINED before present.
        renderer.record(*frameContext);
        renderer.endFrame(*frameContext);
    }
    renderer.finish();

    VulkanDevice& device = renderer.getDevice();
    const VulkanImageView* sceneView = renderer.getSceneImageView();
    EXPECT_NE(sceneView, nullptr);
    VulkanImage& image = sceneView->getImage();
    EXPECT_EQ(image.getFormat(), VK_FORMAT_R32G32B32A32_SFLOAT)
        << "The presented image is not the path-traced accumulation buffer; the scene fell back to rasterization.";

    const VkDeviceSize pixelCount = static_cast<VkDeviceSize>(image.getWidth()) * image.getHeight();
    VulkanBuffer downloadBuffer(
        device, pixelCount * 4 * sizeof(float), VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    device.getGeneralQueue().submitAndWait([&image, &downloadBuffer](VkCommandBuffer cmdBuffer) {
        const VulkanCommandEncoder encoder(cmdBuffer);
        encoder.transitionLayout(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kFragmentSampledRead >> kTransferRead);
        const VkBufferImageCopy region{
            .imageSubresource =
                {.aspectMask = image.getAspectMask(), .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .imageExtent = {image.getWidth(), image.getHeight(), 1},
        };
        encoder.copyImageToBuffer(image, downloadBuffer, region);
        encoder.insertBufferMemoryBarrier(downloadBuffer.createDescriptorInfo(), kTransferWrite >> kHostRead);
    });

    const float* data = downloadBuffer.getHostVisibleData<float>();
    return {data, data + static_cast<size_t>(pixelCount) * 4}; // NOLINT
}

// Relative, because the comparison is against radiance whose scale follows the environment map.
float computeRelativeRmse(const std::span<const float> actual, const std::span<const float> reference) {
    double squaredError = 0.0;
    double squaredReference = 0.0;
    for (size_t i = 0; i < reference.size(); ++i) {
        if (i % 4 == 3) {
            continue;
        }
        const double diff = static_cast<double>(actual[i]) - static_cast<double>(reference[i]);
        squaredError += diff * diff;
        squaredReference += static_cast<double>(reference[i]) * static_cast<double>(reference[i]);
    }
    if (squaredReference == 0.0) {
        return std::sqrt(static_cast<float>(squaredError));
    }
    return static_cast<float>(std::sqrt(squaredError / squaredReference));
}

TEST_F(MaterialExplorerSceneRenderTest, PathTracedViewMatchesGoldenImage) {
    bool supported = false;
    const std::vector<float> pixels = renderPathTracedView(supported);
    if (!supported) {
        GTEST_SKIP() << "The device does not support the path-traced view's ray tracing and descriptor heap.";
    }
    ASSERT_EQ(pixels.size(), static_cast<size_t>(kWidth) * kHeight * 4);

    const auto actualPath = std::filesystem::current_path() / "material-explorer-path-traced-actual.exr";
    ASSERT_TRUE(saveExr(actualPath, pixels, kWidth, kHeight).isValid());

    const auto referencePath = goldenImagePath();
    ASSERT_TRUE(std::filesystem::exists(referencePath))
        << "No reference image at " << referencePath << ".\nRendered output was written to " << actualPath
        << ";\ninspect it and, if correct, copy it to "
           "Crisp/Crisp/Scenes/Test/Data/material-explorer-path-traced.exr.";

    auto referenceResult = loadExr(referencePath);
    ASSERT_TRUE(referenceResult.hasValue());
    const ExrImageData reference = referenceResult.unwrap();
    ASSERT_EQ(reference.width, kWidth);
    ASSERT_EQ(reference.height, kHeight);
    ASSERT_EQ(reference.pixelData.size(), pixels.size());

    const float relativeRmse = computeRelativeRmse(pixels, reference.pixelData);
    EXPECT_LT(relativeRmse, kMaxRelativeRmse)
        << "Rendered output differs from the reference; wrote " << actualPath << " for inspection.";
}

} // namespace
} // namespace crisp
