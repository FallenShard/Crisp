#include <Crisp/Vulkan/VulkanRenderPassBuilder.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

namespace crisp {
namespace {

VkAttachmentDescription2 toAttachmentDescription2(const VkAttachmentDescription& description) {
    return {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .flags = description.flags,
        .format = description.format,
        .samples = description.samples,
        .loadOp = description.loadOp,
        .storeOp = description.storeOp,
        .stencilLoadOp = description.stencilLoadOp,
        .stencilStoreOp = description.stencilStoreOp,
        .initialLayout = description.initialLayout,
        .finalLayout = description.finalLayout,
    };
}

} // namespace

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentCount(const uint32_t count) {
    m_attachments.resize(count);
    m_clearedAttachmentIndices.resize(count, ~0u);
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentDescription(
    const uint32_t attachmentIndex, const VkAttachmentDescription& description) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex] = description;

    if (description.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        m_clearedAttachmentIndices[attachmentIndex] = static_cast<uint32_t>(m_clearValues.size());
        m_clearValues.emplace_back();
    }
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentFormat(
    const uint32_t attachmentIndex, const VkFormat format, const VkSampleCountFlagBits sampleCount) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].format = format;
    m_attachments[attachmentIndex].samples = sampleCount;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentOps(
    const uint32_t attachmentIndex, const VkAttachmentLoadOp loadOp, const VkAttachmentStoreOp storeOp) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].loadOp = loadOp;
    m_attachments[attachmentIndex].storeOp = storeOp;

    if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        m_clearedAttachmentIndices[attachmentIndex] = static_cast<uint32_t>(m_clearValues.size());
        m_clearValues.emplace_back();
    }
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentStencilOps(
    const uint32_t attachmentIndex, const VkAttachmentLoadOp loadOp, const VkAttachmentStoreOp storeOp) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].stencilLoadOp = loadOp;
    m_attachments[attachmentIndex].stencilStoreOp = storeOp;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentLayouts(
    const uint32_t attachmentIndex, const VkImageLayout initialLayout, const VkImageLayout finalLayout) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_attachments.size());
    m_attachments[attachmentIndex].initialLayout = initialLayout;
    m_attachments[attachmentIndex].finalLayout = finalLayout;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentClearValue(
    uint32_t attachmentIndex, const VkClearValue& clearValue) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_clearedAttachmentIndices.size());
    const uint32_t clearIndex = m_clearedAttachmentIndices[attachmentIndex];
    CRISP_CHECK_GE_LT(clearIndex, 0, m_clearValues.size());
    m_clearValues[clearIndex] = clearValue;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentClearColor(
    const uint32_t attachmentIndex, const VkClearColorValue& clearColor) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_clearedAttachmentIndices.size());
    const uint32_t clearIndex = m_clearedAttachmentIndices[attachmentIndex];
    CRISP_CHECK_GE_LT(clearIndex, 0, m_clearValues.size());
    m_clearValues[clearIndex].color = clearColor;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setAttachmentClearDepthStencil(
    uint32_t attachmentIndex, const VkClearDepthStencilValue& clearDepthStencil) {
    CRISP_CHECK_GE_LT(attachmentIndex, 0, m_clearedAttachmentIndices.size());
    const uint32_t clearIndex = m_clearedAttachmentIndices[attachmentIndex];
    CRISP_CHECK_GE_LT(clearIndex, 0, m_clearValues.size());
    m_clearValues[clearIndex].depthStencil = clearDepthStencil;
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setSubpassCount(const uint32_t subpassCount) {
    m_inputAttachmentRefs.resize(subpassCount);
    m_colorAttachmentRefs.resize(subpassCount);
    m_resolveAttachmentRefs.resize(subpassCount);
    m_depthAttachmentRefs.resize(subpassCount);
    m_preserveAttachments.resize(subpassCount);
    m_subpasses.resize(subpassCount, VkSubpassDescription{0, VK_PIPELINE_BIND_POINT_GRAPHICS});
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setSubpassDescription(
    const uint32_t subpass, const VkPipelineBindPoint bindPoint, const VkSubpassDescriptionFlags flags) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    m_subpasses[subpass] = {flags, bindPoint};
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addInputAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const VkImageLayout imageLayout) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    m_inputAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentRefs[subpass].size());
    m_subpasses[subpass].pInputAttachments = m_inputAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addColorAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const std::optional<VkImageLayout> imageLayout) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    CRISP_CHECK_GE_LT(attachment, 0, m_attachments.size());
    m_colorAttachmentRefs[subpass].push_back(
        {attachment, imageLayout ? *imageLayout : m_attachments[attachment].finalLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addResolveAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const VkImageLayout imageLayout) {
    m_resolveAttachmentRefs[subpass].push_back({attachment, imageLayout});
    m_subpasses[subpass].pResolveAttachments = m_resolveAttachmentRefs[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setDepthAttachmentRef(
    const uint32_t subpass, const uint32_t attachment, const std::optional<VkImageLayout> imageLayout) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    CRISP_CHECK_GE_LT(attachment, 0, m_attachments.size());
    m_depthAttachmentRefs[subpass] = {attachment, imageLayout ? *imageLayout : m_attachments[attachment].finalLayout};
    m_subpasses[subpass].pDepthStencilAttachment = &m_depthAttachmentRefs[subpass];
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addPreserveAttachmentRef(uint32_t subpass, uint32_t attachment) {
    CRISP_CHECK_GE_LT(subpass, 0, m_subpasses.size());
    CRISP_CHECK_GE_LT(attachment, 0, m_attachments.size());
    m_preserveAttachments[subpass].push_back(attachment);
    m_subpasses[subpass].preserveAttachmentCount = static_cast<uint32_t>(m_preserveAttachments[subpass].size());
    m_subpasses[subpass].pPreserveAttachments = m_preserveAttachments[subpass].data();
    return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addDependency(
    const uint32_t srcSubpass,
    const uint32_t dstSubpass,
    const VulkanSynchronizationScope& scope,
    const VkDependencyFlags flags) {
    m_dependencyBarriers.push_back({
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = scope.srcStage,
        .srcAccessMask = scope.srcAccess,
        .dstStageMask = scope.dstStage,
        .dstAccessMask = scope.dstAccess,
    });
    // The stage and access masks live entirely in the chained VkMemoryBarrier2; the ones on VkSubpassDependency2
    // are ignored when a barrier is chained, so they stay zero.
    m_dependencies.push_back({
        .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
        .srcSubpass = srcSubpass,
        .dstSubpass = dstSubpass,
        .dependencyFlags = flags,
    });
    return *this;
}

std::unique_ptr<VulkanRenderPass> VulkanRenderPassBuilder::create(
    const VulkanDevice& device, const VkExtent2D& renderArea) const {

    RenderPassParameters params{};
    params.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    params.renderArea = renderArea;
    params.clearValues = m_clearValues;
    params.attachmentDescriptions = m_attachments;

    std::vector<VkAttachmentDescription2> attachments;
    attachments.reserve(m_attachments.size());
    for (const auto& attachment : m_attachments) {
        attachments.push_back(toAttachmentDescription2(attachment));
    }

    const auto toReference2 = [this](const VkAttachmentReference& ref) {
        return VkAttachmentReference2{
            .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
            .attachment = ref.attachment,
            .layout = ref.layout,
            .aspectMask =
                ref.attachment < m_attachments.size()
                    ? determineImageAspect(m_attachments[ref.attachment].format)
                    : VkImageAspectFlags{VK_IMAGE_ASPECT_COLOR_BIT},
        };
    };
    const auto toReferences2 = [&toReference2](const std::vector<VkAttachmentReference>& refs) {
        std::vector<VkAttachmentReference2> result;
        result.reserve(refs.size());
        for (const auto& ref : refs) {
            result.push_back(toReference2(ref));
        }
        return result;
    };

    const size_t subpassCount = m_subpasses.size();
    std::vector<std::vector<VkAttachmentReference2>> inputRefs(subpassCount);
    std::vector<std::vector<VkAttachmentReference2>> colorRefs(subpassCount);
    std::vector<std::vector<VkAttachmentReference2>> resolveRefs(subpassCount);
    std::vector<VkAttachmentReference2> depthRefs(subpassCount);
    std::vector<VkSubpassDescription2> subpasses;
    subpasses.reserve(subpassCount);

    for (size_t i = 0; i < subpassCount; ++i) {
        inputRefs[i] = toReferences2(m_inputAttachmentRefs[i]);
        colorRefs[i] = toReferences2(m_colorAttachmentRefs[i]);
        resolveRefs[i] = toReferences2(m_resolveAttachmentRefs[i]);
        depthRefs[i] = toReference2(m_depthAttachmentRefs[i]);

        const auto& subpass = m_subpasses[i];
        subpasses.push_back({
            .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
            .flags = subpass.flags,
            .pipelineBindPoint = subpass.pipelineBindPoint,
            .inputAttachmentCount = static_cast<uint32_t>(inputRefs[i].size()),
            .pInputAttachments = inputRefs[i].data(),
            .colorAttachmentCount = static_cast<uint32_t>(colorRefs[i].size()),
            .pColorAttachments = colorRefs[i].data(),
            .pResolveAttachments = subpass.pResolveAttachments ? resolveRefs[i].data() : nullptr,
            .pDepthStencilAttachment = subpass.pDepthStencilAttachment ? &depthRefs[i] : nullptr,
            .preserveAttachmentCount = static_cast<uint32_t>(m_preserveAttachments[i].size()),
            .pPreserveAttachments = m_preserveAttachments[i].data(),
        });
    }

    std::vector<VkSubpassDependency2> dependencies(m_dependencies);
    for (size_t i = 0; i < dependencies.size(); ++i) {
        dependencies[i].pNext = &m_dependencyBarriers[i];
    }

    VkRenderPassCreateInfo2 renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VkRenderPass renderPass{VK_NULL_HANDLE};
    VK_CHECK(vkCreateRenderPass2(device.getHandle(), &renderPassInfo, nullptr, &renderPass));
    return std::make_unique<VulkanRenderPass>(device, renderPass, std::move(params));
}

} // namespace crisp