#include <Crisp/Renderer/RenderGraphExperimental.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Utils/Enumerate.hpp>
#include <Crisp/Vulkan/VulkanChecks.hpp>

#include <fstream>

namespace crisp::rg
{
RenderGraph::Builder::Builder(RenderGraph& renderGraph, const RenderGraphPassHandle passHandle)
    : m_renderGraph(renderGraph)
    , m_passHandle(passHandle)
    , m_blackboard(renderGraph.getBlackboard())
{
}

void RenderGraph::Builder::exportTexture(RenderGraphResourceHandle res)
{
    auto& resource = m_renderGraph.getResource(res);
    resource.imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    resource.readPasses.push_back({RenderGraphPassHandle::kExternalPass});
}

void RenderGraph::Builder::readTexture(RenderGraphResourceHandle res)
{
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    resource.imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    auto& accessState = pass.inputAccesses.emplace_back();
    accessState.usageType = ResourceUsageType::Texture;
    accessState.pipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    accessState.access = VK_ACCESS_SHADER_READ_BIT;
}

void RenderGraph::Builder::readBuffer(RenderGraphResourceHandle res)
{
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    auto& accessState = pass.inputAccesses.emplace_back();
    accessState.usageType = ResourceUsageType::Storage;
    accessState.pipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    accessState.access = VK_ACCESS_SHADER_READ_BIT;
}

void RenderGraph::Builder::readAttachment(RenderGraphResourceHandle res)
{
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    resource.imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    auto& accessState = pass.inputAccesses.emplace_back();
    accessState.usageType = ResourceUsageType::Attachment;
    accessState.pipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    accessState.access = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
}

void RenderGraph::Builder::readStorageImage(RenderGraphResourceHandle res)
{
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    resource.imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    auto& accessState = pass.inputAccesses.emplace_back();
    accessState.usageType = ResourceUsageType::Storage;
    accessState.pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    accessState.access = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
}

RenderGraphResourceHandle RenderGraph::Builder::createAttachment(
    const RenderGraphImageDescription& description, std::string&& name, std::optional<VkClearValue> clearValue)
{
    const auto handle = m_renderGraph.addImageResource(description, std::move(name));
    auto& resource = m_renderGraph.getResource(handle);
    resource.producer = m_passHandle;
    resource.clearValue = clearValue;

    const bool isDepthAttachment = isDepthFormat(description.format);
    resource.imageUsageFlags =
        isDepthAttachment ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.outputs.push_back(handle);
    auto& accessState = pass.outputAccesses.emplace_back();
    accessState.usageType = ResourceUsageType::Attachment;
    accessState.pipelineStage =
        isDepthAttachment ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    accessState.access =
        isDepthAttachment ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (isDepthAttachment)
    {
        pass.setDepthAttachment(handle);
    }
    else
    {
        pass.pushColorAttachment(handle);
    }
    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::createStorageImage(
    const RenderGraphImageDescription& description, std::string&& name)
{
    const auto handle = m_renderGraph.addImageResource(description, std::move(name));
    auto& resource = m_renderGraph.getResource(handle);
    resource.producer = m_passHandle;
    resource.imageUsageFlags = VK_IMAGE_USAGE_STORAGE_BIT;

    m_renderGraph.getPass(m_passHandle).outputs.push_back(handle);
    auto& accessState = m_renderGraph.getPass(m_passHandle).outputAccesses.emplace_back();
    accessState.usageType = ResourceUsageType::Storage;
    accessState.pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    accessState.access = VK_ACCESS_SHADER_WRITE_BIT;
    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::createBuffer(
    const RenderGraphBufferDescription& description, std::string&& name)
{
    const auto handle = m_renderGraph.addBufferResource(description, std::move(name));
    m_renderGraph.getResource(handle).producer = m_passHandle;

    m_renderGraph.getPass(m_passHandle).outputs.push_back(handle);
    auto& accessState = m_renderGraph.getPass(m_passHandle).outputAccesses.emplace_back();
    accessState.usageType = ResourceUsageType::Storage;
    accessState.pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    accessState.access = VK_ACCESS_SHADER_WRITE_BIT;
    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::writeAttachment(RenderGraphResourceHandle handle)
{
    readAttachment(handle);
    const auto& inputResource = m_renderGraph.getResource(handle);
    const auto& inputDesc = m_renderGraph.getImageDescription(handle);

    const auto modifiedHandle = createAttachment(inputDesc, std::string(inputResource.name));
    m_renderGraph.getResource(modifiedHandle).producer = m_passHandle;
    m_renderGraph.getResource(modifiedHandle).version++;
    return modifiedHandle;
}

RenderGraphBlackboard& RenderGraph::Builder::getBlackboard()
{
    return m_blackboard;
}

size_t RenderGraph::getPassCount() const
{
    return m_passes.size();
}

size_t RenderGraph::getResourceCount() const
{
    return m_resources.size();
}

std::unique_ptr<VulkanImageView> RenderGraph::createViewFromResource(const RenderGraphResourceHandle handle) const
{
    const auto& res{getResource(handle)};
    return createView(*m_physicalImages[res.physicalResourceIndex].image, VK_IMAGE_VIEW_TYPE_2D);
}

Result<> RenderGraph::toGraphViz(const std::string& path) const
{
    std::ofstream outputFile(path);
    if (!outputFile)
    {
        return resultError("Failed to open output file: {}!", path);
    }

    // Write the Graphviz header
    outputFile << "digraph FrameGraph {" << std::endl;

    for (auto&& [idx, res] : enumerate(m_resources))
    {
        if (res.type == ResourceType::Image)
        {
            outputFile << fmt::format(
                              R"({} [label="{} R: {}, PROD: {}, V: {}", style="filled", shape="{}", color="{}"];)",
                              idx,
                              res.name,
                              res.readPasses.size(),
                              getPass(res.producer).name,
                              res.version,
                              "ellipse",
                              "springgreen")
                       << std::endl;
        }
        else
        {
            outputFile << fmt::format(
                              R"({} [label="{} R: {}, PROD: {}, V: {}", style="filled", shape="{}", color="{}"];)",
                              idx,
                              res.name,
                              res.readPasses.size(),
                              getPass(res.producer).name,
                              res.version,
                              "ellipse",
                              "peachpuff")
                       << std::endl;
        }
    }

    // Iterate over each vertex and its neighbors in the graph
    for (uint32_t i = 0; i < m_passes.size(); ++i)
    {
        const auto& node = m_passes[i];
        const uint32_t vertexIdx = i + static_cast<uint32_t>(m_resources.size());
        // Write the vertex and its label
        outputFile << fmt::format(
                          R"({} [label="{}", style="filled", shape="{}", color="{}"];)",
                          vertexIdx,
                          node.name,
                          "box",
                          "deepskyblue")
                   << std::endl;

        // Iterate over the neighbors of the current vertex
        for (const auto& neighbor : node.inputs)
        {
            // Write the edge between the current vertex and its neighbor
            outputFile << "    " << neighbor.id << " -> " << vertexIdx << ";" << std::endl;
        }

        for (const auto& neighbor : node.outputs)
        {
            // Write the edge between the current vertex and its neighbor
            outputFile << "    " << vertexIdx << " -> " << neighbor.id << ";" << std::endl;
        }
    }

    // Write the Graphviz footer
    outputFile << "}" << std::endl;

    outputFile.close();
    std::cout << "Graph visualization saved to: " << path << std::endl;

    return kResultSuccess;
}

const RenderGraphBlackboard& RenderGraph::getBlackboard() const
{
    return m_blackboard;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::setAttachmentCount(const uint32_t count)
{
    m_attachments.resize(count);
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::setAttachmentOps(
    const uint32_t attachmentIndex, const VkAttachmentLoadOp loadOp, const VkAttachmentStoreOp storeOp)
{
    m_attachments.at(attachmentIndex).loadOp = loadOp;
    m_attachments.at(attachmentIndex).storeOp = storeOp;
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::setAttachmentStencilOps(
    const uint32_t attachmentIndex, const VkAttachmentLoadOp loadOp, const VkAttachmentStoreOp storeOp)
{
    m_attachments.at(attachmentIndex).stencilLoadOp = loadOp;
    m_attachments.at(attachmentIndex).stencilStoreOp = storeOp;
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::setAttachmentLayouts(
    const uint32_t attachmentIndex, const VkImageLayout initialLayout, const VkImageLayout finalLayout)
{
    m_attachments.at(attachmentIndex).initialLayout = initialLayout;
    m_attachments.at(attachmentIndex).finalLayout = finalLayout;
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::setAttachmentFormat(
    const uint32_t attachmentIndex, const VkFormat format, const VkSampleCountFlagBits sampleCount)
{
    m_attachments.at(attachmentIndex).format = format;
    m_attachments.at(attachmentIndex).samples = sampleCount;
    return *this;
}

// Subpass configuration
RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::setSubpassCount(const uint32_t numSubpasses)
{
    m_inputAttachmentRefs.resize(numSubpasses);
    m_colorAttachmentRefs.resize(numSubpasses);
    m_resolveAttachmentRefs.resize(numSubpasses);
    m_depthAttachmentRefs.resize(numSubpasses);
    m_preserveAttachments.resize(numSubpasses);
    m_subpasses.resize(numSubpasses, VkSubpassDescription{0, VK_PIPELINE_BIND_POINT_GRAPHICS});
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::configureSubpass(
    const uint32_t subpass, const VkPipelineBindPoint bindPoint, const VkSubpassDescriptionFlags flags)
{
    m_subpasses.at(subpass) = {flags, bindPoint};
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::addInputAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout)
{
    m_inputAttachmentRefs[subpass].push_back({attachmentIndex, imageLayout});
    m_subpasses[subpass].inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentRefs[subpass].size());
    m_subpasses[subpass].pInputAttachments = m_inputAttachmentRefs[subpass].data();
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::addColorAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout)
{
    m_colorAttachmentRefs[subpass].push_back({attachmentIndex, imageLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::addColorAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex)
{
    m_colorAttachmentRefs[subpass].push_back({attachmentIndex, m_attachments.at(attachmentIndex).finalLayout});
    m_subpasses[subpass].colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentRefs[subpass].size());
    m_subpasses[subpass].pColorAttachments = m_colorAttachmentRefs[subpass].data();
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::addResolveAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout)
{
    m_resolveAttachmentRefs[subpass].push_back({attachmentIndex, imageLayout});
    m_subpasses[subpass].pResolveAttachments = m_resolveAttachmentRefs[subpass].data();
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::setDepthAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex, const VkImageLayout imageLayout)
{
    m_depthAttachmentRefs[subpass] = {attachmentIndex, imageLayout};
    m_subpasses[subpass].pDepthStencilAttachment = &m_depthAttachmentRefs[subpass];
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::addPreserveAttachmentRef(
    const uint32_t subpass, const uint32_t attachmentIndex)
{
    m_preserveAttachments[subpass].push_back(attachmentIndex);
    m_subpasses[subpass].preserveAttachmentCount = static_cast<uint32_t>(m_preserveAttachments[subpass].size());
    m_subpasses[subpass].pPreserveAttachments = m_preserveAttachments[subpass].data();
    return *this;
}

RenderGraph::RenderPassBuilder& RenderGraph::RenderPassBuilder::addDependency(
    const uint32_t srcSubpass,
    const uint32_t dstSubpass,
    const VkPipelineStageFlags srcStageMask,
    const VkAccessFlags srcAccessMask,
    const VkPipelineStageFlags dstStageMask,
    const VkAccessFlags dstAccessMask,
    const VkDependencyFlags flags)
{
    m_dependencies.push_back({srcSubpass, dstSubpass, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, flags});
    return *this;
}

std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> RenderGraph::RenderPassBuilder::create(
    const VkDevice device) const
{
    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
    renderPassInfo.pAttachments = m_attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(m_subpasses.size());
    renderPassInfo.pSubpasses = m_subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(m_dependencies.size());
    renderPassInfo.pDependencies = m_dependencies.data();

    VkRenderPass renderPass{VK_NULL_HANDLE};
    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
    return {renderPass, m_attachments};
}

std::vector<VkImageLayout> RenderGraph::RenderPassBuilder::getFinalLayouts() const
{
    std::vector<VkImageLayout> layouts;
    layouts.reserve(m_attachments.size());
    for (const auto& attachment : m_attachments)
    {
        layouts.push_back(attachment.finalLayout);
    }
    return layouts;
}

VkExtent2D RenderGraph::getRenderArea(const RenderGraphPass& pass, const VkExtent2D swapChainExtent)
{
    VkExtent2D renderArea{0, 0};
    for (const auto& output : pass.outputs)
    {
        const auto& desc = m_imageDescriptions[getResource(output).descriptionIndex];
        const auto dims = calculateImageExtent(desc, swapChainExtent);
        if (renderArea.width != 0)
        {
            CRISP_CHECK_EQ(renderArea.width, dims.width);
        }
        else
        {
            renderArea.width = dims.width;
        }
        if (renderArea.height != 0)
        {
            CRISP_CHECK_EQ(renderArea.height, dims.height);
        }
        else
        {
            renderArea.height = dims.height;
        }
    }
    return renderArea;
}

RenderTargetInfo RenderGraph::toRenderTargetInfo(const RenderGraphImageDescription& desc)
{
    return {
        .format = desc.format,
        .sampleCount = desc.sampleCount,
        .layerCount = desc.layerCount,
        .mipmapCount = desc.mipLevelCount,
        .depthSlices = desc.depth,
        .createFlags = desc.createFlags,
        .size = {desc.width, desc.height},
        .isSwapChainDependent = desc.sizePolicy == SizePolicy::SwapChainRelative,
    };
}

void RenderGraph::compile(
    const VulkanDevice& device, const VkExtent2D& swapChainExtent, const VkCommandBuffer cmdBuffer)
{
    determineAliasedResurces();
    createPhysicalResources(device, swapChainExtent, cmdBuffer);

    m_physicalPasses.clear();
    m_physicalPasses.reserve(m_passes.size());
    for (const auto& pass : m_passes)
    {
        fmt::print("Building render pass: {}\n", pass.name);
        auto& physicalPass = m_physicalPasses.emplace_back();

        RenderPassBuilder builder{};
        builder.setSubpassCount(1)
            .configureSubpass(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
            .setAttachmentCount(static_cast<uint32_t>(
                pass.colorAttachments.size() + (pass.depthStencilAttachment.has_value() ? 1 : 0)));

        std::vector<VkAttachmentReference> colorAttachmentRefs{};
        uint32_t attachmentIndex = 0;
        RenderPassParameters renderPassParams{};
        std::vector<VkClearValue> attachmentClearValues{};
        for (const RenderGraphResourceHandle colorAttachment : pass.colorAttachments)
        {
            const auto& res{getResource(colorAttachment)};
            const VkImageLayout initialLayout = res.imageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT
                                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                    : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            builder
                .setAttachmentFormat(
                    attachmentIndex,
                    m_imageDescriptions[res.descriptionIndex].format,
                    m_imageDescriptions[res.descriptionIndex].sampleCount)
                .setAttachmentStencilOps(
                    attachmentIndex, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE)
                .setAttachmentOps(
                    attachmentIndex,
                    res.clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    res.readPasses.empty() ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE)
                .setAttachmentLayouts(attachmentIndex, initialLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .addColorAttachmentRef(0, attachmentIndex);

            auto rtInfo = toRenderTargetInfo(m_imageDescriptions[res.descriptionIndex]);
            rtInfo.usage = res.imageUsageFlags;
            rtInfo.initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            rtInfo.buffered = false;

            renderPassParams.renderTargetInfos.push_back(rtInfo);
            renderPassParams.renderTargets.push_back(m_physicalImages[res.physicalResourceIndex].image.get());
            renderPassParams.attachmentMappings.push_back(
                {attachmentIndex, renderPassParams.renderTargets.back()->getFullRange(), false});
            attachmentClearValues.push_back(res.clearValue ? *res.clearValue : VkClearValue{});
            ++attachmentIndex;
        }

        if (pass.depthStencilAttachment)
        {
            const auto& res{getResource(*pass.depthStencilAttachment)};
            const VkImageLayout initialLayout = res.imageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT
                                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                    : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            builder
                .setAttachmentFormat(
                    attachmentIndex,
                    m_imageDescriptions[res.descriptionIndex].format,
                    m_imageDescriptions[res.descriptionIndex].sampleCount)
                .setAttachmentStencilOps(
                    attachmentIndex, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE)
                .setAttachmentOps(
                    attachmentIndex,
                    res.clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    res.readPasses.empty() ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE)
                .setAttachmentLayouts(attachmentIndex, initialLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .setDepthAttachmentRef(0, attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            auto rtInfo = toRenderTargetInfo(m_imageDescriptions[res.descriptionIndex]);
            rtInfo.usage = res.imageUsageFlags;
            rtInfo.initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            rtInfo.buffered = false;

            renderPassParams.renderTargetInfos.push_back(rtInfo);
            renderPassParams.renderTargets.push_back(m_physicalImages[res.physicalResourceIndex].image.get());
            renderPassParams.attachmentMappings.push_back(
                {attachmentIndex, renderPassParams.renderTargets.back()->getFullRange(), false});
            attachmentClearValues.push_back(res.clearValue ? *res.clearValue : VkClearValue{});

            ++attachmentIndex;
        }

        builder.addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

        renderPassParams.subpassCount = 1;
        renderPassParams.renderArea = getRenderArea(pass, swapChainExtent);

        auto [passHandle, vkAttachments] = builder.create(device.getHandle());
        renderPassParams.attachmentDescriptions = std::move(vkAttachments);

        physicalPass = std::make_unique<VulkanRenderPass>(
            device, passHandle, std::move(renderPassParams), std::move(attachmentClearValues));

        device.getDebugMarker().setObjectName(physicalPass->getHandle(), pass.name.c_str());
    }
}

void RenderGraph::execute(const VkCommandBuffer cmdBuffer)
{
    RenderPassExecutionContext executionCtx{};
    executionCtx.cmdBuffer = cmdBuffer;
    for (const auto&& [idx, pass] : enumerate(m_passes))
    {
        for (const auto& [inIdx, inputAccess] : enumerate(pass.inputAccesses))
        {
            const auto& res = getResource(pass.inputs[inIdx]);

            if (inputAccess.usageType == ResourceUsageType::Texture)
            {
                const auto& physicalImage{m_physicalImages.at(res.physicalResourceIndex)};
                const bool isDepthAttachment = isDepthFormat(m_imageDescriptions[res.descriptionIndex].format);
                physicalImage.image->transitionLayout(
                    executionCtx.cmdBuffer,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    isDepthAttachment ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                      : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    inputAccess.pipelineStage);
            }
        }

        auto& physPass = *m_physicalPasses.at(idx);
        physPass.begin(executionCtx.cmdBuffer, 0, VK_SUBPASS_CONTENTS_INLINE);

        pass.executeFunc(executionCtx);

        physPass.end(executionCtx.cmdBuffer, 0);
    }
}

void RenderGraph::determineAliasedResurces()
{
    const auto timelines{calculateResourceTimelines()};
    std::vector<bool> processed(m_resources.size(), false);
    uint16_t currPhysBufferIdx{0};
    uint16_t currPhysImageIdx{0};
    for (auto&& [idx, resource] : enumerate(m_resources))
    {
        if (processed[idx])
        {
            continue;
        }

        processed[idx] = true;

        const auto findResourcesToAlias = [&](const auto& descriptions, auto& physicalResource)
        {
            uint32_t lastReadPassIdx = timelines[idx].lastRead;
            for (uint32_t j = static_cast<uint32_t>(idx) + 1; j < m_resources.size(); ++j)
            {
                if (lastReadPassIdx >= timelines[j].firstWrite)
                {
                    continue;
                }
                if (resource.type == m_resources[j].type)
                {
                    if (descriptions[resource.descriptionIndex].canAlias(descriptions[m_resources[j].descriptionIndex]))
                    {
                        lastReadPassIdx = timelines[j].lastRead;
                        m_resources[j].physicalResourceIndex = resource.physicalResourceIndex;
                        processed[j] = true;
                        spdlog::info("Aliasing {} with {}", m_resources[idx].name, m_resources[j].name);
                        physicalResource.aliasedResourceIndices.push_back(j);
                    }
                }
            }
        };

        if (resource.type == ResourceType::Buffer)
        {
            resource.physicalResourceIndex = currPhysBufferIdx++;
            auto& desc = m_physicalBuffers.emplace_back();
            desc.descriptionIndex = resource.descriptionIndex;
            desc.aliasedResourceIndices.push_back(static_cast<uint32_t>(idx));
            findResourcesToAlias(m_bufferDescriptions, desc);
        }
        else
        {
            resource.physicalResourceIndex = currPhysImageIdx++;
            auto& desc = m_physicalImages.emplace_back();
            desc.descriptionIndex = resource.descriptionIndex;
            desc.aliasedResourceIndices.push_back(static_cast<uint32_t>(idx));
            findResourcesToAlias(m_imageDescriptions, desc);
        }
    }

    spdlog::info("{} physical buffer(s), {} physical image(s).", currPhysBufferIdx, currPhysImageIdx);
}

void RenderGraph::createPhysicalResources(
    const VulkanDevice& device, const VkExtent2D swapChainExtent, const VkCommandBuffer cmdBuffer)
{
    const auto determineUsageFlags = [this](const std::vector<uint32_t>& imageResourceIndices) -> VkImageUsageFlags
    {
        VkImageUsageFlags flags{0};
        for (uint32_t idx : imageResourceIndices)
        {
            flags = flags | m_resources[idx].imageUsageFlags;
        }
        return flags;
    };

    const auto determineCreateFlags = [this](const std::vector<uint32_t>& imageResourceIndices) -> VkImageUsageFlags
    {
        VkImageCreateFlags flags{0};
        for (uint32_t idx : imageResourceIndices)
        {
            flags = flags | m_imageDescriptions[m_resources[idx].descriptionIndex].createFlags;
        }
        return flags;
    };

    const auto determineInitialLayout = [this](const RenderGraphPhysicalImage& image, VkImageUsageFlags usageFlags)
        -> std::tuple<VkImageLayout, VkPipelineStageFlagBits>
    {
        if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
        }
        if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
        {
            return {VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
        }

        if (isDepthFormat(m_imageDescriptions[image.descriptionIndex].format))
        {
            return {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT};
        }

        return {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    };

    for (auto& res : m_physicalImages)
    {
        const auto& desc = m_imageDescriptions[res.descriptionIndex];
        const auto usageFlags = determineUsageFlags(res.aliasedResourceIndices);

        const uint32_t layerMultiplier = 1; // buffered ? RendererConfig::VirtualFrameCount : 1;
        res.image = std::make_unique<VulkanImage>(
            device,
            calculateImageExtent(desc, swapChainExtent),
            desc.layerCount * layerMultiplier,
            desc.mipLevelCount,
            desc.format,
            usageFlags,
            determineCreateFlags(res.aliasedResourceIndices));

        const auto [initialLayout, pipelineStage] = determineInitialLayout(res, usageFlags);
        res.image->transitionLayout(cmdBuffer, initialLayout, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipelineStage);
        device.getDebugMarker().setObjectName(
            res.image->getHandle(), m_resources[res.aliasedResourceIndices[0]].name.c_str());
    }

    for (auto& res : m_physicalBuffers)
    {
        const auto& desc = m_bufferDescriptions[res.descriptionIndex];

        const uint32_t sizeMultiplier = 1; // buffered ? RendererConfig::VirtualFrameCount : 1;
        res.buffer = std::make_unique<VulkanBuffer>(
            device, desc.size * sizeMultiplier, desc.usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
}

RenderGraphBlackboard& RenderGraph::getBlackboard()
{
    return m_blackboard;
}

const VulkanRenderPass& RenderGraph::getRenderPass(const std::string& name) const
{
    return *m_physicalPasses[m_passMap.at(name).id];
}

void RenderGraph::resize(const uint32_t width, const uint32_t height) const // NOLINT
{
    spdlog::critical("Not yet implemented! New size: {} x {}", width, height);
}

std::vector<RenderGraph::ResourceTimeline> RenderGraph::calculateResourceTimelines()
{
    FlatHashMap<std::string, ResourceTimeline> unversionedTimelines;
    for (const auto& res : m_resources)
    {
        unversionedTimelines[res.name] = {};
    }

    for (auto&& [passIdx, pass] : enumerate(m_passes))
    {
        for (const auto& in : pass.inputs)
        {
            auto& tl = unversionedTimelines[getResource(in).name];
            tl.lastRead = std::max(tl.lastRead, static_cast<uint32_t>(passIdx));
        }

        for (const auto& out : pass.outputs)
        {
            auto& tl = unversionedTimelines[getResource(out).name];
            tl.firstWrite = std::min(tl.firstWrite, static_cast<uint32_t>(passIdx));
        }
    }

    std::vector<ResourceTimeline> timelines(m_resources.size());
    for (auto&& [idx, t] : enumerate(timelines))
    {
        t = unversionedTimelines[m_resources[idx].name];
    }

    for (auto&& [idx, t] : enumerate(timelines))
    {
        spdlog::warn(
            "{}. {}-{}: W: {} ({}), R: {} ({})",
            idx,
            m_resources[idx].name,
            m_resources[idx].version,
            t.firstWrite,
            t.firstWrite < m_passes.size() ? m_passes[t.firstWrite].name : "None",
            t.lastRead,
            t.lastRead == 0 ? "None" : m_passes[t.lastRead].name);
    }

    return timelines;
}

RenderGraphResourceHandle RenderGraph::addImageResource(
    const RenderGraphImageDescription& description, std::string&& name)
{
    m_imageDescriptions.push_back(description);

    auto& res = m_resources.emplace_back();
    res.type = ResourceType::Image;
    res.name = std::move(name);
    res.descriptionIndex = static_cast<uint16_t>(m_imageDescriptions.size()) - 1;

    return {static_cast<uint32_t>(m_resources.size()) - 1};
}

RenderGraphResourceHandle RenderGraph::addBufferResource(
    const RenderGraphBufferDescription& description, std::string&& name)
{
    m_bufferDescriptions.push_back(description);

    auto& res = m_resources.emplace_back();
    res.type = ResourceType::Buffer;
    res.name = std::move(name);
    res.descriptionIndex = static_cast<uint16_t>(m_bufferDescriptions.size()) - 1;

    return {static_cast<uint32_t>(m_resources.size()) - 1};
}

RenderGraphPass& RenderGraph::getPass(const RenderGraphPassHandle handle)
{
    return m_passes[handle.id];
}

const RenderGraphPass& RenderGraph::getPass(const RenderGraphPassHandle handle) const
{
    return m_passes[handle.id];
}

RenderGraphResource& RenderGraph::getResource(const RenderGraphResourceHandle handle)
{
    return m_resources[handle.id];
}

const RenderGraphResource& RenderGraph::getResource(const RenderGraphResourceHandle handle) const
{
    return m_resources[handle.id];
}

const RenderGraphImageDescription& RenderGraph::getImageDescription(const RenderGraphResourceHandle handle)
{
    return m_imageDescriptions[getResource(handle).physicalResourceIndex];
}
} // namespace crisp::rg