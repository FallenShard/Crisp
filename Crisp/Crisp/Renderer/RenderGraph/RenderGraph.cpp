#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

#include <ranges>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

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
    pass.inputAccesses.push_back({.usageType = ResourceUsageType::Texture, .stage = kFragmentRead});
}

void RenderGraph::Builder::readBuffer(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back({.usageType = ResourceUsageType::Storage, .stage = kFragmentRead});
}

void RenderGraph::Builder::readAttachment(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back({.usageType = ResourceUsageType::Attachment, .stage = kFragmentInputRead});
}

void RenderGraph::Builder::readStorageImage(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back({.usageType = ResourceUsageType::Storage, .stage = kComputeRead});
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
    resource.producerAccess.stage.stage =
        isDepthAttachment ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    resource.producerAccess.stage.access =
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
    resource.producerAccess = {.usageType = ResourceUsageType::Storage, .stage = kComputeWrite};

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
    m_renderGraph.getResource(handle).producerAccess = {.usageType = ResourceUsageType::Storage, .stage = kComputeWrite};

    return handle;
}

RenderGraphResourceHandle RenderGraph::Builder::createBuffer(
    const RenderGraphBufferDescription& description, std::string&& name) {
    CRISP_CHECK_EQ(description.externalBuffer, VK_NULL_HANDLE);

    const auto handle = m_renderGraph.addBufferResource(description, std::move(name), /*isExternal=*/false);
    m_renderGraph.getResource(handle).producer = m_passHandle;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.outputs.push_back(handle);
    m_renderGraph.getResource(handle).producerAccess = {.usageType = ResourceUsageType::Storage, .stage = kComputeWrite};

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

void RenderGraph::compile(const VulkanDevice& device, const VkExtent2D& swapChainExtent) {
    CRISP_LOGI("Compiling RenderGraph...");
    determineAliasedResurces();
    device.getGeneralQueue().submitAndWait([this, &device, &swapChainExtent](const VkCommandBuffer cmdBuffer) {
        createPhysicalResources(device, swapChainExtent, cmdBuffer);
    });
    createPhysicalPasses(device, swapChainExtent);
    CRISP_LOGI("RenderGraph compiled!");
}

void RenderGraph::execute(const FrameContext& frameContext) {
    const auto synchronizeInputResources = [this](const RenderGraphPass& pass, const FrameContext& ctx) {
        for (const auto& [inIdx, inputAccess] : std::views::enumerate(pass.inputAccesses)) {
            const auto& res = getResource(pass.inputs[inIdx]);

            if (res.type == ResourceType::Image) {
                const auto& physicalImage{m_physicalImages.at(res.physicalResourceIndex)};
                const VkImageLayout newLayout =
                    inputAccess.usageType == ResourceUsageType::Texture
                        ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        : VK_IMAGE_LAYOUT_GENERAL;
                ctx.commandEncoder.transitionLayout(
                    *physicalImage.image, newLayout, res.producerAccess.stage >> inputAccess.stage);
            } else if (res.type == ResourceType::Buffer) {
                const auto& physicalBuffer{m_physicalBuffers.at(res.physicalResourceIndex)};
                ctx.commandEncoder.insertBufferMemoryBarrier(
                    *physicalBuffer.buffer, res.producerAccess.stage >> inputAccess.stage);
            }
        }
    };

    const auto& encoder{frameContext.commandEncoder};

    for (const auto&& [idx, pass] : std::views::enumerate(m_passes)) {
        // CRISP_LOGI("Executing pass: {}", pass.name);
        if (pass.type == PassType::Rasterizer) {
            synchronizeInputResources(pass, frameContext);

            auto& renderPass = *m_physicalPasses.at(m_physicalPassIndices.at(idx));
            auto& framebuffer = *m_framebuffers.at(m_physicalPassIndices.at(idx));
            encoder.beginRenderPass(renderPass, framebuffer);
            pass.executeFunc(frameContext);
            encoder.endRenderPass(renderPass);

            std::vector<const VulkanImageView*> attachmentViews{};
            for (const RenderGraphResourceHandle resourceId : pass.colorAttachments) {
                const auto& colorImageResource{getResource(resourceId)};
                attachmentViews.push_back(m_imageViews.at(colorImageResource.physicalResourceIndex).get());
            }
            if (pass.depthStencilAttachment) {
                const auto& depthResource{getResource(*pass.depthStencilAttachment)};
                attachmentViews.push_back(m_imageViews.at(depthResource.physicalResourceIndex).get());
            }
            for (const auto& [i, attachmentView] : std::views::enumerate(attachmentViews)) {
                attachmentView->getImage().setImageLayout(
                    renderPass.getFinalLayout(i), attachmentView->getSubresourceRange());
            }

            // for (const auto& [i, attachmentView] : std::views::enumerate(m_attachmentViews)) {
            //     // We unconditionally set the layout here because the render pass did an automatic layout transition.
            //     attachmentView->getImage().setImageLayout(
            //         m_params.attachmentDescriptions.at(i).finalLayout, attachmentView->getSubresourceRange());
            // }
        } else if (pass.type == PassType::Compute || pass.type == PassType::RayTracing) {
            // for (const auto resHandle : pass.outputs) {
            //     const auto& res = getResource(resHandle);
            //     const auto& outputAccess = res.producerAccess;

            //     // if (res.type == ResourceType::Image) {
            //     //     const auto& physicalImage{m_physicalImages.at(res.physicalResourceIndex)};
            //     //     if (outputAccess.usageType == ResourceUsageType::Texture) {
            //     //         physicalImage.image->transitionLayoutDirect(
            //     //             executionCtx.cmdBuffer.getHandle(),
            //     //             // glayouts[res.physicalResourceIndex],
            //     //             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            //     //             glastPipelineStage[res.physicalResourceIndex],
            //     //             glastAccessFlags[res.physicalResourceIndex],
            //     //             // res.producerAccess.pipelineStage,
            //     //             // res.producerAccess.access,
            //     //             outputAccess.pipelineStage,
            //     //             outputAccess.access);
            //     //     } else if (outputAccess.usageType == ResourceUsageType::Storage) {
            //     //         physicalImage.image->transitionLayoutDirect(
            //     //             executionCtx.cmdBuffer.getHandle(),
            //     //             // glayouts[res.physicalResourceIndex],
            //     //             VK_IMAGE_LAYOUT_GENERAL,
            //     //             glastPipelineStage[res.physicalResourceIndex],
            //     //             glastAccessFlags[res.physicalResourceIndex],
            //     //             // res.producerAccess.pipelineStage,
            //     //             // res.producerAccess.access,
            //     //             outputAccess.pipelineStage,
            //     //             outputAccess.access);
            //     //     }
            //     //     // glayouts[res.physicalResourceIndex] =
            //     //     //     outputAccess.usageType == ResourceUsageType::Texture
            //     //     //         ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            //     //     //         : VK_IMAGE_LAYOUT_GENERAL;
            //     //     glastPipelineStage[res.physicalResourceIndex] = outputAccess.pipelineStage;
            //     //     glastAccessFlags[res.physicalResourceIndex] = outputAccess.access;
            //     // } else if (res.type == ResourceType::Buffer) {
            //     //     const auto& physicalBuffer{m_physicalBuffers.at(res.physicalResourceIndex)};
            //     //     executionCtx.cmdBuffer.insertBufferMemoryBarrier(
            //     //         physicalBuffer.buffer->createDescriptorInfo(),
            //     //         res.producerAccess.pipelineStage,
            //     //         res.producerAccess.access,
            //     //         outputAccess.pipelineStage,
            //     //         outputAccess.access);
            //     // }
            // }

            synchronizeInputResources(pass, frameContext);
            pass.executeFunc(frameContext);
        }
    }
}

RenderGraphBlackboard& RenderGraph::getBlackboard() {
    return m_blackboard;
}

const VulkanRenderPass& RenderGraph::getRenderPass(const std::string& name) const {
    return *m_physicalPasses.at(m_physicalPassIndices.at(m_passMap.at(name).id));
}

const VulkanImageView& RenderGraph::getImageView(const std::string& name, const uint32_t attachmentIndex) const {
    const auto passHandle = m_passMap.at(name);
    const auto& pass = getPass(passHandle);
    uint32_t currIndex = 0;
    for (const RenderGraphResourceHandle resourceId : pass.colorAttachments) {
        if (currIndex == attachmentIndex) {
            return *m_imageViews.at(getResource(resourceId).physicalResourceIndex);
        }
        ++currIndex;
    }
    if (pass.depthStencilAttachment) {
        if (currIndex == attachmentIndex) {
            return *m_imageViews.at(getResource(*pass.depthStencilAttachment).physicalResourceIndex);
        }
        ++currIndex;
    }

    CRISP_FATAL("Attachment index {} out of range for pass {}.", attachmentIndex, name);
}

void RenderGraph::resize(const VulkanDevice& device, const VkExtent2D swapChainExtent) // NOLINT
{
    compile(device, swapChainExtent);
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
        CRISP_LOGI(
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

std::pair<VkImageLayout, VulkanSynchronizationStage> RenderGraph::determineInitialLayout(
    const RenderGraphPhysicalImage& image, const VkImageUsageFlags usageFlags) {
    if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
        return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kFragmentSampledRead};
    }
    if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT) {
        return {VK_IMAGE_LAYOUT_GENERAL, kComputeRead};
    }

    if (isDepthFormat(m_imageDescriptions[image.descriptionIndex].format)) {
        return {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, kDepthWrite};
    }

    return {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, kColorWrite};
};

void RenderGraph::determineAliasedResurces() {
    CRISP_LOGI("Determining resources to alias...");
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

    CRISP_LOGI("{} physical buffer(s), {} physical image(s).", currPhysBufferIdx, currPhysImageIdx);
}

void RenderGraph::createPhysicalResources(
    const VulkanDevice& device, const VkExtent2D swapChainExtent, const VkCommandBuffer cmdBuffer) {
    CRISP_LOGI("Creating physical resources...");
    const VulkanCommandEncoder commandEncoder{cmdBuffer};
    for (auto& physicalImage : m_physicalImages) {
        const auto& desc = m_imageDescriptions[physicalImage.descriptionIndex];
        physicalImage.image = std::make_unique<VulkanImage>(
            device,
            calculateImageExtent(desc, swapChainExtent),
            desc.layerCount,
            desc.mipLevelCount,
            desc.format,
            determineUsageFlags(physicalImage.aliasedResourceIndices),
            determineCreateFlags(physicalImage.aliasedResourceIndices));
        device.setObjectName(*physicalImage.image, m_resources[physicalImage.aliasedResourceIndices[0]].name.c_str());
        CRISP_LOGT(
            "Created image: {} with size: {}x{}",
            m_resources[physicalImage.aliasedResourceIndices[0]].name,
            physicalImage.image->getWidth(),
            physicalImage.image->getHeight());

        // TODO(fallenshard): Looks like a hack.
        const auto lastUsageFlags = getImageDescription({physicalImage.aliasedResourceIndices.back()}).imageUsageFlags;
        const auto [initialLayout, stage] = determineInitialLayout(physicalImage, lastUsageFlags);
        commandEncoder.transitionLayout(*physicalImage.image, initialLayout, kNullStage >> stage);
    }

    for (const auto& res : m_resources) {
        if (res.type == ResourceType::Image) {
            const auto physicalResourceIndex = res.physicalResourceIndex;
            m_imageViews[physicalResourceIndex] =
                createView(device, *m_physicalImages[physicalResourceIndex].image, VK_IMAGE_VIEW_TYPE_2D);
        }
    }

    for (auto& res : m_physicalBuffers) {
        const auto& desc = m_bufferDescriptions[res.descriptionIndex];
        res.buffer =
            std::make_unique<VulkanBuffer>(device, desc.size, desc.usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        device.setObjectName(res.buffer->getHandle(), m_resources[res.aliasedResourceIndices[0]].name.c_str());
    }
}

void RenderGraph::createPhysicalPasses(const VulkanDevice& device, const VkExtent2D swapChainExtent) {
    CRISP_LOGI("Creating physical passes...");
    m_physicalPassIndices.clear();
    m_physicalPassIndices.resize(m_passes.size(), -1);
    m_physicalPasses.clear();
    m_physicalPasses.reserve(m_passes.size());
    m_framebuffers.clear();
    for (auto&& [idx, pass] : std::views::enumerate(m_passes)) {
        if (pass.type != PassType::Rasterizer) {
            CRISP_LOGI("Skipping pass {} as it's not a rasterizer pass.", pass.name);
            continue;
        }
        CRISP_LOGD("Building render pass: {}\n", pass.name);

        RenderPassBuilder builder{};
        builder.setSubpassCount(1).setAttachmentCount(pass.getAttachmentCount());

        std::vector<VkAttachmentReference> colorAttachmentRefs{};
        uint32_t attachmentIndex = 0;
        RenderPassCreationParams creationParams{};
        for (const RenderGraphResourceHandle resourceId : pass.colorAttachments) {
            const auto& colorImageResource{getResource(resourceId)};
            const auto& colorDescription{getImageDescription(resourceId)};
            builder
                .setAttachmentDescription(
                    attachmentIndex, toColorAttachmentDescription(colorImageResource, colorDescription))
                .addColorAttachmentRef(0, attachmentIndex);

            if (colorDescription.clearValue) {
                creationParams.clearValues.push_back(*colorDescription.clearValue);
            }
            ++attachmentIndex;
        }

        if (pass.depthStencilAttachment) {
            const auto& depthResource{getResource(*pass.depthStencilAttachment)};
            const auto& depthDescription{getImageDescription(*pass.depthStencilAttachment)};
            builder
                .setAttachmentDescription(attachmentIndex, toDepthAttachmentDescription(depthResource, depthDescription))
                .setDepthAttachmentRef(0, attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            if (depthDescription.clearValue) {
                creationParams.clearValues.push_back(*depthDescription.clearValue);
            }

            // Ensure that we are synchronizing the load.
            const VkAttachmentLoadOp loadOp =
                depthDescription.clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                builder.addDependency(VK_SUBPASS_EXTERNAL, 0, kNullStage >> kDepthWrite);
            }

            ++attachmentIndex;
        }

        builder.addDependency(VK_SUBPASS_EXTERNAL, 0, kFragmentRead >> kColorWrite);

        auto& physicalPass =
            m_physicalPasses.emplace_back(builder.create(device, getRenderArea(pass, swapChainExtent), creationParams));
        m_physicalPassIndices[idx] = static_cast<int32_t>(m_physicalPasses.size()) - 1;
        device.setObjectName(*physicalPass, pass.name.c_str());

        std::vector<VkImageView> attachmentViews{};
        for (const RenderGraphResourceHandle resourceId : pass.colorAttachments) {
            const auto& colorImageResource{getResource(resourceId)};
            attachmentViews.push_back(m_imageViews.at(colorImageResource.physicalResourceIndex)->getHandle());
        }
        if (pass.depthStencilAttachment) {
            const auto& depthResource{getResource(*pass.depthStencilAttachment)};
            attachmentViews.push_back(m_imageViews.at(depthResource.physicalResourceIndex)->getHandle());
        }

        m_framebuffers.push_back(std::make_unique<VulkanFramebuffer>(
            device, physicalPass->getHandle(), physicalPass->getRenderArea(), attachmentViews));
        device.setObjectName(*m_framebuffers.back(), fmt::format("{}Framebuffer", pass.name).c_str());
    }
}

} // namespace crisp::rg