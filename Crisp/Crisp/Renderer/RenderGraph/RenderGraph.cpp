#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

#include <ranges>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/VulkanRenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanChecks.hpp>

namespace crisp::rg {
namespace {

const auto logger = createLoggerMt("RenderGraph");

RenderTargetInfo toRenderTargetInfo(const RenderGraphImageDescription& desc) {
    return {
        .initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .format = desc.format,
        .sampleCount = desc.sampleCount,
        .layerCount = desc.layerCount,
        .mipmapCount = desc.mipLevelCount,
        .depthSlices = desc.depth,
        .createFlags = desc.createFlags,
        .usage = desc.imageUsageFlags,
        .buffered = false,
        .size = {desc.width, desc.height},
        .isSwapChainDependent = desc.sizePolicy == SizePolicy::SwapChainRelative,
    };
}

VkAttachmentDescription toAttachmentDescription(
    const RenderGraphResource& resource,
    const RenderGraphImageDescription& imageDescription,
    const VkImageLayout optimalAttachmentLayout) {
    return {
        .format = imageDescription.format,
        .samples = imageDescription.sampleCount,
        .loadOp = imageDescription.clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = resource.readPasses.empty() ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout =
            imageDescription.imageUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT
                ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                : optimalAttachmentLayout,
        .finalLayout = optimalAttachmentLayout,
    };
}

VkAttachmentDescription toColorAttachmentDescription(
    const RenderGraphResource& resource, const RenderGraphImageDescription& imageDescription) {
    return toAttachmentDescription(resource, imageDescription, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

VkAttachmentDescription toDepthAttachmentDescription(
    const RenderGraphResource& resource, const RenderGraphImageDescription& imageDescription) {
    return toAttachmentDescription(resource, imageDescription, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

} // namespace

RenderGraph::Builder::Builder(RenderGraph& renderGraph, const RenderGraphPassHandle passHandle)
    : m_renderGraph(renderGraph)
    , m_passHandle(passHandle) {}

void RenderGraph::Builder::exportTexture(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back({RenderGraphPassHandle::kExternalPass});
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
}

void RenderGraph::Builder::readTexture(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back({
        .usageType = ResourceUsageType::Texture,
        .pipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .access = VK_ACCESS_SHADER_READ_BIT,
    });
}

void RenderGraph::Builder::readBuffer(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back({
        .usageType = ResourceUsageType::Storage,
        .pipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .access = VK_ACCESS_SHADER_READ_BIT,
    });
}

void RenderGraph::Builder::readAttachment(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back({
        .usageType = ResourceUsageType::Attachment,
        .pipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .access = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
    });
}

void RenderGraph::Builder::readStorageImage(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back({
        .usageType = ResourceUsageType::Storage,
        .pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .access = VK_ACCESS_SHADER_READ_BIT,
    });
}

RenderGraphResourceHandle RenderGraph::Builder::createAttachment(
    const RenderGraphImageDescription& description, std::string&& name, std::optional<VkClearValue> clearValue) {
    const auto handle = m_renderGraph.addImageResource(description, std::move(name));
    auto& resource = m_renderGraph.getResource(handle);
    resource.producer = m_passHandle;
    m_renderGraph.getImageDescription(handle).clearValue = clearValue;

    const bool isDepthAttachment = isDepthFormat(description.format);
    m_renderGraph.getImageDescription(handle).imageUsageFlags =
        isDepthAttachment ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.outputs.push_back(handle);
    resource.producerAccess.usageType = ResourceUsageType::Attachment;
    resource.producerAccess.pipelineStage =
        isDepthAttachment ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    resource.producerAccess.access =
        isDepthAttachment ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (isDepthAttachment) {
        pass.depthStencilAttachment = handle;
    } else {
        pass.colorAttachments.push_back(handle);
    }
    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::createStorageImage(
    const RenderGraphImageDescription& description, std::string&& name) {
    const auto handle = m_renderGraph.addImageResource(description, std::move(name));
    auto& resource = m_renderGraph.getResource(handle);
    resource.producer = m_passHandle;
    resource.producerAccess = {
        .usageType = ResourceUsageType::Storage,
        .pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .access = VK_ACCESS_SHADER_WRITE_BIT,
    };

    m_renderGraph.getImageDescription(handle).imageUsageFlags = VK_IMAGE_USAGE_STORAGE_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.outputs.push_back(handle);

    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::importBuffer(
    const RenderGraphBufferDescription& description, std::string&& name) {
    CRISP_CHECK_NE(description.externalBuffer, VK_NULL_HANDLE);

    const auto handle = m_renderGraph.addBufferResource(description, std::move(name), /*isExternal=*/true);
    m_renderGraph.getResource(handle).producer = m_passHandle;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.outputs.push_back(handle);
    m_renderGraph.getResource(handle).producerAccess = {
        .usageType = ResourceUsageType::Storage,
        .pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .access = VK_ACCESS_SHADER_WRITE_BIT,
    };

    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::createBuffer(
    const RenderGraphBufferDescription& description, std::string&& name) {
    CRISP_CHECK_EQ(description.externalBuffer, VK_NULL_HANDLE);

    const auto handle = m_renderGraph.addBufferResource(description, std::move(name), /*isExternal=*/false);
    m_renderGraph.getResource(handle).producer = m_passHandle;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.outputs.push_back(handle);
    m_renderGraph.getResource(handle).producerAccess = {
        .usageType = ResourceUsageType::Storage,
        .pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .access = VK_ACCESS_SHADER_WRITE_BIT,
    };

    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::writeAttachment(RenderGraphResourceHandle handle) {
    readAttachment(handle);
    const auto& inputResource = m_renderGraph.getResource(handle);
    const auto& inputDesc = m_renderGraph.getImageDescription(handle);

    const auto modifiedHandle = createAttachment(inputDesc, std::string(inputResource.name));
    m_renderGraph.getResource(modifiedHandle).producer = m_passHandle;
    m_renderGraph.getResource(modifiedHandle).version++;
    return modifiedHandle;
}

RenderGraphBlackboard& RenderGraph::Builder::getBlackboard() {
    return m_renderGraph.getBlackboard();
}

void RenderGraph::Builder::setType(PassType type) {
    m_renderGraph.getPass(m_passHandle).type = type;
}

size_t RenderGraph::getPassCount() const {
    return m_passes.size();
}

size_t RenderGraph::getResourceCount() const {
    return m_resources.size();
}

std::unique_ptr<VulkanImageView> RenderGraph::createViewFromResource(
    const VulkanDevice& device, const RenderGraphResourceHandle handle) const {
    const auto& res{getResource(handle)};
    return createView(device, *m_physicalImages[res.physicalResourceIndex].image, VK_IMAGE_VIEW_TYPE_2D);
}

const VulkanImageView& RenderGraph::getResourceImageView(RenderGraphResourceHandle handle) const {
    const auto& res{getResource(handle)};
    CRISP_CHECK_EQ(res.type, ResourceType::Image);
    return *m_imageViews.at(res.physicalResourceIndex);
}

const RenderGraphBlackboard& RenderGraph::getBlackboard() const {
    return m_blackboard;
}

VkExtent2D RenderGraph::getRenderArea(const RenderGraphPass& pass, const VkExtent2D swapChainExtent) {
    VkExtent2D renderArea{0, 0};
    for (const auto& output : pass.outputs) {
        const auto& desc = m_imageDescriptions[getResource(output).descriptionIndex];
        const auto dims = calculateImageExtent(desc, swapChainExtent);
        if (renderArea.width != 0) {
            CRISP_CHECK_EQ(renderArea.width, dims.width);
        } else {
            renderArea.width = dims.width;
        }
        if (renderArea.height != 0) {
            CRISP_CHECK_EQ(renderArea.height, dims.height);
        } else {
            renderArea.height = dims.height;
        }
    }
    return renderArea;
}

void RenderGraph::compile(
    const VulkanDevice& device, const VkExtent2D& swapChainExtent, const VkCommandBuffer cmdBuffer) {
    determineAliasedResurces();
    createPhysicalResources(device, swapChainExtent, cmdBuffer);
    createPhysicalPasses(device, swapChainExtent);
}

std::vector<VkPipelineStageFlags> glastPipelineStage;
std::vector<VkPipelineStageFlags> glastAccessFlags;

void RenderGraph::execute(const VkCommandBuffer cmdBuffer, const uint32_t virtualFrameIndex, VkDevice device) {
    std::vector<bool> isInReadState(m_resources.size(), false);
    // VkPipelineStageFlags m_lastSynchronizedPipelineStage{0};
    // VkPipelineStageFlags m_lastSynchronizedAccessStage{0};

    const auto synchronizeInputResources =
        [this, &isInReadState](const RenderGraphPass& pass, const RenderPassExecutionContext& ctx) {
            for (const auto& [inIdx, inputAccess] : std::views::enumerate(pass.inputAccesses)) {
                // if (isInReadState.at(pass.inputs[inIdx].id)) {
                //     continue;
                // }
                // isInReadState.at(pass.inputs[inIdx].id) = true;

                const auto& res = getResource(pass.inputs[inIdx]);

                if (res.type == ResourceType::Image) {
                    const auto& physicalImage{m_physicalImages.at(res.physicalResourceIndex)};
                    if (inputAccess.usageType == ResourceUsageType::Texture) {
                        physicalImage.image->transitionLayoutDirect(
                            ctx.cmdBuffer.getHandle(),
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            glastPipelineStage[res.physicalResourceIndex],
                            glastAccessFlags[res.physicalResourceIndex],
                            // res.producerAccess.pipelineStage,
                            // res.producerAccess.access,
                            inputAccess.pipelineStage,
                            inputAccess.access);
                    } else if (inputAccess.usageType == ResourceUsageType::Storage) {
                        physicalImage.image->transitionLayoutDirect(
                            ctx.cmdBuffer.getHandle(),
                            // glayouts[res.physicalResourceIndex],
                            VK_IMAGE_LAYOUT_GENERAL,
                            glastPipelineStage[res.physicalResourceIndex],
                            glastAccessFlags[res.physicalResourceIndex],
                            // res.producerAccess.pipelineStage,
                            // res.producerAccess.access,
                            inputAccess.pipelineStage,
                            inputAccess.access);
                    }
                    glastPipelineStage[res.physicalResourceIndex] = inputAccess.pipelineStage;
                    glastAccessFlags[res.physicalResourceIndex] = inputAccess.access;
                } else if (res.type == ResourceType::Buffer) {
                    const auto& physicalBuffer{m_physicalBuffers.at(res.physicalResourceIndex)};
                    ctx.cmdBuffer.insertBufferMemoryBarrier(
                        physicalBuffer.buffer->createDescriptorInfo(),
                        res.producerAccess.pipelineStage,
                        res.producerAccess.access,
                        inputAccess.pipelineStage,
                        inputAccess.access);
                }
            }
        };

    const RenderPassExecutionContext executionCtx{VulkanCommandBuffer{cmdBuffer}, virtualFrameIndex};
    for (const auto&& [idx, pass] : std::views::enumerate(m_passes)) {
        // logger->info("Executing {}, pass: {}", virtualFrameIndex, pass.name);
        if (pass.type == PassType::Rasterizer) {
            synchronizeInputResources(pass, executionCtx);

            auto& renderPass = *m_physicalPasses.at(m_physicalPassIndices.at(idx));
            renderPass.begin(executionCtx.cmdBuffer.getHandle(), 0, VK_SUBPASS_CONTENTS_INLINE);
            pass.executeFunc(executionCtx);
            renderPass.end(executionCtx.cmdBuffer.getHandle(), 0);
        } else if (pass.type == PassType::Compute || pass.type == PassType::RayTracing) {
            for (const auto resHandle : pass.outputs) {
                const auto& res = getResource(resHandle);
                const auto& outputAccess = res.producerAccess;

                if (res.type == ResourceType::Image) {
                    const auto& physicalImage{m_physicalImages.at(res.physicalResourceIndex)};
                    if (outputAccess.usageType == ResourceUsageType::Texture) {
                        physicalImage.image->transitionLayoutDirect(
                            executionCtx.cmdBuffer.getHandle(),
                            // glayouts[res.physicalResourceIndex],
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            glastPipelineStage[res.physicalResourceIndex],
                            glastAccessFlags[res.physicalResourceIndex],
                            // res.producerAccess.pipelineStage,
                            // res.producerAccess.access,
                            outputAccess.pipelineStage,
                            outputAccess.access);
                    } else if (outputAccess.usageType == ResourceUsageType::Storage) {
                        physicalImage.image->transitionLayoutDirect(
                            executionCtx.cmdBuffer.getHandle(),
                            // glayouts[res.physicalResourceIndex],
                            VK_IMAGE_LAYOUT_GENERAL,
                            glastPipelineStage[res.physicalResourceIndex],
                            glastAccessFlags[res.physicalResourceIndex],
                            // res.producerAccess.pipelineStage,
                            // res.producerAccess.access,
                            outputAccess.pipelineStage,
                            outputAccess.access);
                    }
                    // glayouts[res.physicalResourceIndex] =
                    //     outputAccess.usageType == ResourceUsageType::Texture
                    //         ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    //         : VK_IMAGE_LAYOUT_GENERAL;
                    glastPipelineStage[res.physicalResourceIndex] = outputAccess.pipelineStage;
                    glastAccessFlags[res.physicalResourceIndex] = outputAccess.access;
                } else if (res.type == ResourceType::Buffer) {
                    const auto& physicalBuffer{m_physicalBuffers.at(res.physicalResourceIndex)};
                    executionCtx.cmdBuffer.insertBufferMemoryBarrier(
                        physicalBuffer.buffer->createDescriptorInfo(),
                        res.producerAccess.pipelineStage,
                        res.producerAccess.access,
                        outputAccess.pipelineStage,
                        outputAccess.access);
                }
            }

            synchronizeInputResources(pass, executionCtx);
            pass.executeFunc(executionCtx);
        }
    }
}

RenderGraphBlackboard& RenderGraph::getBlackboard() {
    return m_blackboard;
}

const VulkanRenderPass& RenderGraph::getRenderPass(const std::string& name) const {
    return *m_physicalPasses.at(m_physicalPassIndices.at(m_passMap.at(name).id));
}

void RenderGraph::resize(
    const VulkanDevice& device, const VkExtent2D swapChainExtent, const VkCommandBuffer cmdBuffer) // NOLINT
{
    determineAliasedResurces();
    createPhysicalResources(device, swapChainExtent, cmdBuffer);
    createPhysicalPasses(device, swapChainExtent);
}

std::vector<RenderGraph::ResourceTimeline> RenderGraph::calculateResourceTimelines() {
    FlatHashMap<std::string, ResourceTimeline> unversionedTimelines;
    for (const auto& res : m_resources) {
        unversionedTimelines[res.name] = {};
    }

    for (auto&& [passIdx, pass] : std::views::enumerate(m_passes)) {
        for (const auto& in : pass.inputs) {
            auto& tl = unversionedTimelines[getResource(in).name];
            tl.lastRead = std::max(tl.lastRead, static_cast<uint32_t>(passIdx));
        }

        for (const auto& out : pass.outputs) {
            auto& tl = unversionedTimelines[getResource(out).name];
            tl.firstWrite = std::min(tl.firstWrite, static_cast<uint32_t>(passIdx));
        }
    }

    std::vector<ResourceTimeline> timelines(m_resources.size());
    for (auto&& [idx, t] : std::views::enumerate(timelines)) {
        t = unversionedTimelines[m_resources[idx].name];
    }

    for (auto&& [idx, t] : std::views::enumerate(timelines)) {
        logger->info(
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
    const RenderGraphImageDescription& description, std::string&& name) {
    m_imageDescriptions.push_back(description);

    auto& res = m_resources.emplace_back();
    res.type = ResourceType::Image;
    res.name = std::move(name);
    res.descriptionIndex = static_cast<uint16_t>(m_imageDescriptions.size()) - 1;

    return {static_cast<uint32_t>(m_resources.size()) - 1};
}

RenderGraphResourceHandle RenderGraph::addBufferResource(
    const RenderGraphBufferDescription& description, std::string&& name, const bool isExternal) {
    m_bufferDescriptions.push_back(description);

    auto& res = m_resources.emplace_back();
    res.type = ResourceType::Buffer;
    res.name = std::move(name);
    res.descriptionIndex = static_cast<uint16_t>(m_bufferDescriptions.size()) - 1;
    res.isExternal = isExternal;

    return {static_cast<uint32_t>(m_resources.size()) - 1};
}

VkImageUsageFlags RenderGraph::determineUsageFlags(const std::vector<uint32_t>& imageResourceIndices) const {
    VkImageUsageFlags flags{0};
    for (const uint32_t idx : imageResourceIndices) {
        flags = flags | getImageDescription({idx}).imageUsageFlags;
    }
    return flags;
}

VkImageCreateFlags RenderGraph::determineCreateFlags(const std::vector<uint32_t>& imageResourceIndices) const {
    VkImageCreateFlags flags{0};
    for (const uint32_t idx : imageResourceIndices) {
        flags = flags | getImageDescription({idx}).createFlags;
    }
    return flags;
};

std::tuple<VkImageLayout, VkAccessFlagBits, VkPipelineStageFlagBits> RenderGraph::determineInitialLayout(
    const RenderGraphPhysicalImage& image, VkImageUsageFlags usageFlags) {
    if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
        return {
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    }
    if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT) {
        return {VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    }

    if (isDepthFormat(m_imageDescriptions[image.descriptionIndex].format)) {
        return {
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT};
    }

    return {
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
};

void RenderGraph::determineAliasedResurces() {
    const auto timelines{calculateResourceTimelines()};
    std::vector<bool> processed(m_resources.size(), false);
    uint16_t currPhysBufferIdx{0};
    uint16_t currPhysImageIdx{0};
    for (auto&& [idx, resource] : std::views::enumerate(m_resources)) {
        if (processed[idx]) {
            continue;
        }

        processed[idx] = true;

        if (resource.isExternal) {
            continue;
        }

        const auto findResourcesToAlias = [&](const auto& descriptions, auto& physicalResource) {
            uint32_t lastReadPassIdx = timelines[idx].lastRead;
            for (uint32_t j = static_cast<uint32_t>(idx) + 1; j < m_resources.size(); ++j) {
                if (m_resources[j].isExternal) {
                    continue;
                }
                if (lastReadPassIdx >= timelines[j].firstWrite) {
                    continue;
                }
                if (resource.type == m_resources[j].type) {
                    if (descriptions[resource.descriptionIndex].canAlias(descriptions[m_resources[j].descriptionIndex])) {
                        lastReadPassIdx = timelines[j].lastRead;
                        m_resources[j].physicalResourceIndex = resource.physicalResourceIndex;
                        processed[j] = true;
                        spdlog::info("Aliasing {} with {}", m_resources[idx].name, m_resources[j].name);
                        physicalResource.aliasedResourceIndices.push_back(j);
                    }
                }
            }
        };

        if (resource.type == ResourceType::Buffer) {
            resource.physicalResourceIndex = currPhysBufferIdx++;
            auto& desc = m_physicalBuffers.emplace_back();
            desc.descriptionIndex = resource.descriptionIndex;
            desc.aliasedResourceIndices.push_back(static_cast<uint32_t>(idx));
            findResourcesToAlias(m_bufferDescriptions, desc);
        } else {
            resource.physicalResourceIndex = currPhysImageIdx++;
            auto& desc = m_physicalImages.emplace_back();
            desc.descriptionIndex = resource.descriptionIndex;
            desc.aliasedResourceIndices.push_back(static_cast<uint32_t>(idx));
            findResourcesToAlias(m_imageDescriptions, desc);
        }
    }

    logger->info("{} physical buffer(s), {} physical image(s).", currPhysBufferIdx, currPhysImageIdx);
}

void RenderGraph::createPhysicalResources(
    const VulkanDevice& device, const VkExtent2D swapChainExtent, const VkCommandBuffer cmdBuffer) {
    int imgIdx = 0;
    glastPipelineStage.resize(m_physicalImages.size());
    glastAccessFlags.resize(m_physicalImages.size());
    for (auto& res : m_physicalImages) {
        const auto& desc = m_imageDescriptions[res.descriptionIndex];
        const auto usageFlags = determineUsageFlags(res.aliasedResourceIndices);

        const uint32_t layerMultiplier = 1; // buffered ? kRendererVirtualFrameCount : 1;
        res.image = std::make_unique<VulkanImage>(
            device,
            calculateImageExtent(desc, swapChainExtent),
            desc.layerCount * layerMultiplier,
            desc.mipLevelCount,
            desc.format,
            usageFlags,
            determineCreateFlags(res.aliasedResourceIndices));
        res.image->setTag(m_resources[res.aliasedResourceIndices[0]].name);
        device.getDebugMarker().setObjectName(
            res.image->getHandle(), m_resources[res.aliasedResourceIndices[0]].name.c_str());

        const auto lastUsageFlags = getImageDescription({res.aliasedResourceIndices.back()}).imageUsageFlags;
        const auto [initialLayout, access, pipelineStage] = determineInitialLayout(res, lastUsageFlags);
        res.image->transitionLayoutDirect(
            cmdBuffer, initialLayout, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, pipelineStage, access);
        glastPipelineStage[imgIdx] = pipelineStage;
        glastAccessFlags[imgIdx] = access;
        imgIdx++;
    }

    for (const auto& res : m_resources) {
        if (res.type == ResourceType::Image) {
            m_imageViews[res.physicalResourceIndex] =
                createView(device, *m_physicalImages[res.physicalResourceIndex].image, VK_IMAGE_VIEW_TYPE_2D);
        }
    }

    for (auto& res : m_physicalBuffers) {
        const auto& desc = m_bufferDescriptions[res.descriptionIndex];

        const uint32_t sizeMultiplier = 1; // buffered ? kRendererVirtualFrameCount : 1;
        res.buffer = std::make_unique<VulkanBuffer>(
            device, desc.size * sizeMultiplier, desc.usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        device.getDebugMarker().setObjectName(
            res.buffer->getHandle(), m_resources[res.aliasedResourceIndices[0]].name.c_str());
    }
}

void RenderGraph::createPhysicalPasses(const VulkanDevice& device, const VkExtent2D swapChainExtent) {
    m_physicalPassIndices.clear();
    m_physicalPassIndices.resize(m_passes.size(), -1);
    m_physicalPasses.clear();
    m_physicalPasses.reserve(m_passes.size());
    for (auto&& [idx, pass] : std::views::enumerate(m_passes)) {
        if (pass.type != PassType::Rasterizer) {
            logger->info("Skipping pass {} as it's not a rasterizer pass.", pass.name);
            continue;
        }
        logger->debug("Building render pass: {}\n", pass.name);

        VulkanRenderPassBuilder builder{};
        builder.setSubpassCount(1)
            .configureSubpass(0, VK_PIPELINE_BIND_POINT_GRAPHICS)
            .setAttachmentCount(pass.getAttachmentCount());

        std::vector<VkAttachmentReference> colorAttachmentRefs{};
        uint32_t attachmentIndex = 0;
        RenderPassParameters renderPassParams{};
        std::vector<VkClearValue> attachmentClearValues{};
        for (const RenderGraphResourceHandle resourceId : pass.colorAttachments) {
            const auto& colorImageResource{getResource(resourceId)};
            const auto& colorDescription{getImageDescription(resourceId)};
            builder.setAttachment(attachmentIndex, toColorAttachmentDescription(colorImageResource, colorDescription))
                .addColorAttachmentRef(0, attachmentIndex);

            renderPassParams.renderTargetInfos.push_back(toRenderTargetInfo(colorDescription));
            renderPassParams.renderTargets.push_back(
                m_physicalImages[colorImageResource.physicalResourceIndex].image.get());
            renderPassParams.attachmentMappings.push_back(
                {attachmentIndex, renderPassParams.renderTargets.back()->getFullRange(), false});
            attachmentClearValues.push_back(colorDescription.clearValue ? *colorDescription.clearValue : VkClearValue{});
            ++attachmentIndex;
        }

        if (pass.depthStencilAttachment) {
            const auto& depthResource{getResource(*pass.depthStencilAttachment)};
            const auto& depthDescription{getImageDescription(*pass.depthStencilAttachment)};
            builder.setAttachment(attachmentIndex, toDepthAttachmentDescription(depthResource, depthDescription))
                .setDepthAttachmentRef(0, attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            renderPassParams.renderTargetInfos.push_back(toRenderTargetInfo(depthDescription));
            renderPassParams.renderTargets.push_back(m_physicalImages[depthResource.physicalResourceIndex].image.get());
            renderPassParams.attachmentMappings.push_back(
                {attachmentIndex, renderPassParams.renderTargets.back()->getFullRange(), false});
            attachmentClearValues.push_back(depthDescription.clearValue ? *depthDescription.clearValue : VkClearValue{});

            // Ensure that we are synchronizing the load.
            const VkAttachmentLoadOp loadOp =
                depthDescription.clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                builder.addDependency({
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    .srcAccessMask = 0,
                    .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                });
            }

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

        auto& physicalPass = m_physicalPasses.emplace_back(std::make_unique<VulkanRenderPass>(
            device, passHandle, std::move(renderPassParams), std::move(attachmentClearValues)));
        m_physicalPassIndices[idx] = static_cast<int32_t>(m_physicalPasses.size()) - 1;

        device.getDebugMarker().setObjectName(physicalPass->getHandle(), pass.name.c_str());
    }
}

} // namespace crisp::rg