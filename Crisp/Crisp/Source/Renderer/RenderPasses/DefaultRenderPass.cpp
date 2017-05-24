#include "DefaultRenderPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanSwapChain.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    DefaultRenderPass::DefaultRenderPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_colorFormat(m_renderer->getSwapChain()->getImageFormat())
        , m_depthFormat(m_renderer->getContext()->findSupportedDepthFormat())
        , m_clearValues(2)
    {
        m_clearValues[0].color        = { 0.5f, 0.1f, 0.1f, 1.0f };
        m_clearValues[1].depthStencil = { 1.0f, 0 };

        createRenderPass();
        createResources();
    }

    DefaultRenderPass::~DefaultRenderPass()
    {
        vkDestroyRenderPass(m_device->getHandle(), m_renderPass, nullptr);
        freeResources();
    }

    void DefaultRenderPass::begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_renderPass;
        renderPassInfo.framebuffer       = framebuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage DefaultRenderPass::getColorAttachment(unsigned int index) const
    {
        return VK_NULL_HANDLE;
    }

    VkImageView DefaultRenderPass::getAttachmentView(unsigned int index, unsigned int frameIndex) const
    {
        return VK_NULL_HANDLE;
    }

    void DefaultRenderPass::createRenderPass()
    {
        // Description for color attachment
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format         = m_colorFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        //// Description for depth attachment
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format         = m_depthFormat;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subPass = {};
        subPass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subPass.colorAttachmentCount    = 1;
        subPass.pColorAttachments       = &colorAttachmentRef;
        subPass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subPass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_renderPass);
    }

    void DefaultRenderPass::createResources()
    {
        // Uses image resources given by swap chain
    }

    void DefaultRenderPass::freeResources()
    {
    }
}