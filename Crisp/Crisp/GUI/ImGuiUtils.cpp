#include <Crisp/Gui/ImGuiUtils.hpp>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace crisp::gui {
namespace {
VkDescriptorPool imGuiPool{VK_NULL_HANDLE};
} // namespace

void initImGui(
    GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    const VulkanDevice& device,
    const uint32_t swapChainImageCount,
    const VkRenderPass renderPass,
    const std::optional<std::string>& fontPath) {
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

    VK_CHECK(vkCreateDescriptorPool(device.getHandle(), &poolInfo, nullptr, &imGuiPool));

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device.getHandle();
    initInfo.QueueFamily = device.getGeneralQueue().getFamilyIndex();
    initInfo.Queue = device.getGeneralQueue().getHandle();
    initInfo.DescriptorPool = imGuiPool;
    initInfo.MinImageCount = swapChainImageCount;
    initInfo.ImageCount = swapChainImageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_LoadFunctions(
        [](const char* funcName, void* userData) {
            return vkGetInstanceProcAddr(static_cast<VkInstance>(userData), funcName);
        },
        initInfo.Instance);
    ImGui_ImplVulkan_Init(&initInfo, renderPass);

    if (fontPath) {
        ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath->c_str(), 16);
    }

    device.getGeneralQueue().submitAndWait([&](VkCommandBuffer commandBuffer) {
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    });
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void shutdownImGui(const VkDevice device) {
    if (imGuiPool) {
        vkDestroyDescriptorPool(device, imGuiPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
    }
}

void prepareImGui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void renderImGui(const VkCommandBuffer cmdBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

// void renderImGuiFrame(Renderer& renderer) {
//     ImGui::Render();
//     renderer.enqueueDefaultPassDrawCommand([](VkCommandBuffer cmdBuffer) {
//         ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
//     });
// }
} // namespace crisp::gui
