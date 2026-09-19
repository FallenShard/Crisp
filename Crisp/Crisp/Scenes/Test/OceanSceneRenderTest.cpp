#include <Crisp/Scenes/OceanScene.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Image/Io/Exr.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

#include <gmock/gmock.h>

#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace crisp {
namespace {

constexpr uint32_t kWidth = 640;
constexpr uint32_t kHeight = 360;

// Long enough for the camera damping to settle and the spectrum to leave its degenerate t = 0.
constexpr uint32_t kWarmupFrames = 8;
constexpr uint32_t kResizeFrame = 3;
constexpr float kFixedDeltaTime = 1.0f / 60.0f;

// Float32 butterflies and texture filtering vary across vendors, so compare scale, not bits.
constexpr float kMaxRelativeRmse = 0.02f;

std::filesystem::path goldenImagePath() {
    return std::filesystem::path{CRISP_TEST_ASSET_DIR} / "ocean-reference.exr";
}

class OceanSceneRenderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        spdlog::set_level(spdlog::level::warn);
        glfwInit();
    }

    static void TearDownTestSuite() {
        glfwTerminate();
    }
};

std::vector<float> renderOceanScene() {
    Window window(
        glm::ivec2{0, 0},
        glm::ivec2{static_cast<int32_t>(kWidth), static_cast<int32_t>(kHeight)},
        "ocean_render_test",
        WindowVisibility::Hidden);

    VulkanCoreParams coreParams{
        .requiredInstanceExtensions = ApplicationEnvironment::getRequiredVulkanInstanceExtensions(),
        .deviceFeatureRequests = createDefaultFeatureRequests(),
        .presentationMode = PresentationMode::DoubleBuffered,
        .includeValidation = true,
    };
    addPageableMemoryFeatures(coreParams.deviceFeatureRequests);
    addMeshShadingFeatures(coreParams.deviceFeatureRequests);
    Renderer renderer(
        std::move(coreParams),
        window.createSurfaceCallback(),
        AssetPaths{
            .shaderSourceDir = CRISP_SHADER_SOURCE_DIR,
            .resourceDir = CRISP_RESOURCE_DIR,
            .spvShaderDir = std::filesystem::path{CRISP_RESOURCE_DIR} / "Shaders",
            .outputDir = std::filesystem::current_path(),
        });

    OceanScene scene(&renderer, &window);

    for (uint32_t frameIdx = 0; frameIdx < kWarmupFrames; ++frameIdx) {
        // Recompiling reallocates every physical image, so anything holding a graph view has to
        // rebind it. Done mid-flight on purpose: lazily built materials only exist by now.
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

    // The tonemap attachment is R16G16B16A16_SFLOAT; unpacked to float below for the EXR.
    const VkDeviceSize pixelCount = static_cast<VkDeviceSize>(image.getWidth()) * image.getHeight();
    VulkanBuffer downloadBuffer(
        device, pixelCount * 4 * sizeof(uint16_t), VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

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

    const uint16_t* halfPixels = downloadBuffer.getHostVisibleData<uint16_t>();
    std::vector<float> pixels(static_cast<size_t>(pixelCount) * 4);
    for (size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = glm::unpackHalf1x16(halfPixels[i]); // NOLINT
    }
    return pixels;
}

// Still linear: GammaCorrect runs in the present pass, which a headless render never reaches.
void savePreviewPpm(const std::filesystem::path& path, const std::span<const float> pixels, const uint32_t width) {
    const auto encodeSrgb = [](const float linear) {
        const float c = std::clamp(linear, 0.0f, 1.0f);
        const float encoded = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        return static_cast<uint8_t>(std::lround(encoded * 255.0f));
    };

    const size_t pixelCount = pixels.size() / 4;
    std::string body;
    body.reserve(pixelCount * 3);
    for (size_t i = 0; i < pixelCount; ++i) {
        for (size_t channel = 0; channel < 3; ++channel) {
            body.push_back(static_cast<char>(encodeSrgb(pixels[i * 4 + channel])));
        }
    }

    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << width << " " << pixelCount / width << "\n255\n";
    file.write(body.data(), static_cast<std::streamsize>(body.size()));
}

// Relative because the ocean's radiance scale moves with exposure and spectrum amplitude.
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

TEST_F(OceanSceneRenderTest, MatchesGoldenImage) {
    const std::vector<float> pixels = renderOceanScene();
    ASSERT_EQ(pixels.size(), static_cast<size_t>(kWidth) * kHeight * 4);

    const auto actualPath = std::filesystem::current_path() / "ocean-actual.exr";
    ASSERT_TRUE(saveExr(actualPath, pixels, kWidth, kHeight).isValid());
    savePreviewPpm(std::filesystem::current_path() / "ocean-actual.ppm", pixels, kWidth);

    const auto referencePath = goldenImagePath();
    ASSERT_TRUE(std::filesystem::exists(referencePath))
        << "No reference image at " << referencePath << ".\nRendered output was written to " << actualPath
        << ";\ninspect it and, if correct, copy it to Crisp/Crisp/Scenes/Test/Data/ocean-reference.exr.";

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
