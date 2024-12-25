#include <Crisp/Gui/ImGuiUtils.hpp>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace crisp::gui {
namespace {
VkDescriptorPool imGuiPool{VK_NULL_HANDLE};
} // namespace

void initImGui(GLFWwindow* window, Renderer& renderer, const std::optional<std::string>& fontPath) {
    VkDescriptorPoolSize poolSizes[] = /* NOLINT */ {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes; // NOLINT

    VK_CHECK(vkCreateDescriptorPool(renderer.getDevice().getHandle(), &poolInfo, nullptr, &imGuiPool));

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = renderer.getInstance().getHandle();
    initInfo.PhysicalDevice = renderer.getPhysicalDevice().getHandle();
    initInfo.Device = renderer.getDevice().getHandle();
    initInfo.Queue = renderer.getDevice().getGeneralQueue().getHandle();
    initInfo.DescriptorPool = imGuiPool;
    initInfo.MinImageCount = kRendererVirtualFrameCount;
    initInfo.ImageCount = kRendererVirtualFrameCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_LoadFunctions(
        [](const char* funcName, void* userData) {
            return vkGetInstanceProcAddr(static_cast<VkInstance>(userData), funcName);
        },
        initInfo.Instance);
    ImGui_ImplVulkan_Init(&initInfo, renderer.getDefaultRenderPass().getHandle());

    if (fontPath) {
        ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath->c_str(), 16);
    }

    auto& device = renderer.getDevice();
    device.postResourceUpdate([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });
    device.waitIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void shutdownImGui(Renderer& renderer) {
    if (imGuiPool) {
        vkDestroyDescriptorPool(renderer.getDevice().getHandle(), imGuiPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
    }
}

void prepareImGuiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void renderImGuiFrame(Renderer& renderer) {
    ImGui::Render();
    renderer.enqueueDefaultPassDrawCommand([](VkCommandBuffer cmdBuffer) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
    });
}
} // namespace crisp::gui
