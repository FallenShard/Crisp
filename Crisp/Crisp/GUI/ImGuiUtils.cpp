#include <Crisp/Gui/ImGuiUtils.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

namespace crisp::gui {
namespace {
VkDescriptorPool imGuiPool{VK_NULL_HANDLE};

void setupTheme(ImGuiStyle& style) {
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.09f, 0.10f, 0.94f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.12f, 0.14f, 0.50f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.10f, 0.12f, 0.94f);

    style.Colors[ImGuiCol_Header] = ImVec4(0.09f, 0.45f, 0.54f, 0.45f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.12f, 0.72f, 0.85f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.12f, 0.78f, 0.92f, 1.00f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.52f, 0.61f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.78f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.08f, 0.65f, 0.76f, 1.00f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.18f, 0.20f, 0.54f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.67f, 0.79f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.12f, 0.67f, 0.79f, 0.67f);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.07f, 0.16f, 0.18f, 0.86f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.12f, 0.67f, 0.79f, 0.80f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.10f, 0.54f, 0.63f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.16f, 0.18f, 0.97f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.08f, 0.32f, 0.38f, 1.00f);

    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.13f, 0.15f, 0.94f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.34f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.13f, 0.15f, 0.75f);

    style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.95f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.56f, 1.00f);

    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.12f, 0.80f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.11f, 0.64f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.12f, 0.80f, 0.94f, 1.00f);

    style.Colors[ImGuiCol_Separator] = ImVec4(0.15f, 0.27f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.65f, 0.75f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.70f, 0.80f, 1.00f);

    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.07f, 0.08f, 0.54f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.34f, 0.36f, 0.54f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.40f, 0.42f, 0.54f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.56f, 0.59f, 0.54f);

    style.Colors[ImGuiCol_Border] = ImVec4(0.18f, 0.24f, 0.27f, 0.50f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.12f, 0.64f, 0.76f, 0.25f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.12f, 0.64f, 0.76f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.12f, 0.64f, 0.76f, 0.95f);

    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.70f, 0.72f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.12f, 0.75f, 0.87f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.20f, 0.70f, 0.85f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.12f, 0.80f, 0.94f, 1.00f);

    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.05f, 0.06f, 0.35f);

    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(5.0f, 3.0f);
    style.ItemSpacing = ImVec2(6.0f, 6.0f);

    style.IndentSpacing = 20.0f;
    style.GrabMinSize = 8.0f;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
}
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

    setupTheme(ImGui::GetStyle());
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
