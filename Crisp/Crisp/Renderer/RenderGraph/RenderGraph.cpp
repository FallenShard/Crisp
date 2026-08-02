#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

#include <ranges>

#include <Crisp/Core/Checks.hpp>

namespace crisp::rg {
namespace {

const auto logger = createLoggerMt("RenderGraph");

VkRenderingAttachmentInfo createRenderingAttachmentInfo(
    const RenderGraphResource& resource,
    const RenderGraphImageDescription& imageDescription,
    const VulkanImageView& imageView,
    const VkImageLayout imageLayout) {
    return {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = imageView.getHandle(),
        .imageLayout = imageLayout,
        .loadOp = imageDescription.clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = resource.readPasses.empty() ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = imageDescription.clearValue.value_or(VkClearValue{}),
    };
}

std::string createPhysicalResourceDebugName(
    const std::string_view type,
    const std::vector<RenderGraphResource>& resources,
    const std::vector<uint32_t>& aliasedResourceIndices) {
    std::string name = fmt::format("RenderGraph {} [", type);
    for (size_t i = 0; i < aliasedResourceIndices.size(); ++i) {
        if (i > 0) {
            name += ", ";
        }
        name += resources.at(aliasedResourceIndices[i]).name;
    }
    name += "]";
    return name;
}

VulkanSynchronizationStage getSampledImageReadAccess(const PassType passType) {
    switch (passType) {
    case PassType::Compute:
        return kComputeRead;
    case PassType::RayTracing:
        return kRayTracingRead;
    case PassType::Rasterizer:
        return kFragmentSampledRead;
    }
    CRISP_FATAL("Unsupported render graph pass type.");
}

VulkanSynchronizationStage getStorageImageReadAccess(const PassType passType) {
    switch (passType) {
    case PassType::Compute:
        return kComputeStorageRead;
    case PassType::RayTracing:
        return kRayTracingStorageRead;
    case PassType::Rasterizer:
        return kFragmentRead;
    }
    CRISP_FATAL("Unsupported render graph pass type.");
}

VulkanSynchronizationStage getStorageImageWriteAccess(const PassType passType) {
    switch (passType) {
    case PassType::Compute:
        return kComputeStorageWrite;
    case PassType::RayTracing:
        return kRayTracingStorageWrite;
    case PassType::Rasterizer:
        CRISP_FATAL("Rasterization passes cannot create storage-image outputs.");
    }
    CRISP_FATAL("Unsupported render graph pass type.");
}

} // namespace

RenderGraph::Builder::Builder(RenderGraph& renderGraph, const RenderGraphPassHandle passHandle)
    : m_renderGraph(renderGraph)
    , m_passHandle(passHandle) {}

void RenderGraph::Builder::exportTexture(
    const RenderGraphResourceHandle res, const VulkanSynchronizationStage externalAccess) {
    auto& resource = m_renderGraph.getResource(res);
    CRISP_CHECK_EQ(resource.type, ResourceType::Image);
    resource.readPasses.push_back({RenderGraphPassHandle::kExternalPass});
    resource.externalAccess = externalAccess;
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
}

void RenderGraph::Builder::readTexture(RenderGraphResourceHandle res) {
    auto& resource = m_renderGraph.getResource(res);
    resource.readPasses.push_back(m_passHandle);
    m_renderGraph.getImageDescription(res).imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

    auto& pass = m_renderGraph.getPass(m_passHandle);
    pass.inputs.push_back(res);
    pass.inputAccesses.push_back(
        {.usageType = ResourceUsageType::Texture, .stage = getSampledImageReadAccess(pass.type)});
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
    pass.inputAccesses.push_back(
        {.usageType = ResourceUsageType::Storage, .stage = getStorageImageReadAccess(pass.type)});
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
    resource.producerAccess = {
        .usageType = ResourceUsageType::Attachment,
        .stage = isDepthAttachment ? kDepthWrite : kColorWrite,
    };
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
        .stage = getStorageImageWriteAccess(m_renderGraph.getPass(m_passHandle).type),
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
    const auto& desc = getImageDescription(handle);
    auto& image = *m_physicalImages[res.physicalResourceIndex].image;
    const auto imageType = desc.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
    auto view = createView(device, image, getImageViewType(imageType, image.getLayerCount(), false));
    device.setObjectName(*view, fmt::format("RenderGraph {} Auxiliary View", res.name));
    return view;
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
    CRISP_LOGD("Compiling RenderGraph...");
    m_swapChainExtent = swapChainExtent;
    determineAliasedResurces();
    device.getGeneralQueue().submitAndWait([this, &device, &swapChainExtent](const VkCommandBuffer cmdBuffer) {
        createPhysicalResources(device, swapChainExtent, cmdBuffer);
    });
    m_passProfiler.initialize(device, m_passes.size());
    CRISP_LOGI(
        "RenderGraph compiled: {} pass(es), {} physical image(s), {} physical buffer(s).",
        m_passes.size(),
        m_physicalImages.size(),
        m_physicalBuffers.size());
}

void RenderGraph::execute(const FrameContext& frameContext) {
    const auto& encoder{frameContext.commandEncoder};
    const auto synchronizeImageAccess =
        [this, &encoder](
            const RenderGraphResource& resource,
            const VkImageLayout newLayout,
            const VulkanSynchronizationStage access,
            const bool isWrite,
            const VkImageSubresourceRange& range) {
            auto& physicalImage = m_physicalImages.at(resource.physicalResourceIndex);
            auto& image = *physicalImage.image;
            const bool layoutChanges = image.getLayout(range.baseArrayLayer, range.baseMipLevel) != newLayout;
            const bool requiresBarrier = isWrite || physicalImage.lastAccessWasWrite || layoutChanges;

            // Read-after-read in an unchanged layout requires no barrier. All other cases either carry a memory
            // dependency or perform a layout transition.
            if (requiresBarrier) {
                auto destinationAccess = access;
                if (!isWrite && layoutChanges) {
                    if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                        destinationAccess = destinationAccess | physicalImage.shaderReadAccess;
                    } else if (newLayout == VK_IMAGE_LAYOUT_GENERAL) {
                        destinationAccess = destinationAccess | physicalImage.generalReadAccess;
                    }
                }
                encoder.transitionLayout(image, newLayout, physicalImage.lastAccess >> destinationAccess, range);
            }

            if (isWrite) {
                physicalImage.lastAccess = access;
                physicalImage.lastAccessWasWrite = true;
            } else {
                physicalImage.lastAccess = requiresBarrier ? access : physicalImage.lastAccess | access;
                physicalImage.lastAccessWasWrite = false;
            }
        };

    const auto synchronizeInputResources =
        [this, &synchronizeImageAccess](const RenderGraphPass& pass, const FrameContext& ctx) {
            for (const auto& [inIdx, inputAccess] : std::views::enumerate(pass.inputAccesses)) {
                const auto& res = getResource(pass.inputs[inIdx]);

                if (res.type == ResourceType::Image) {
                    const VkImageLayout newLayout =
                        inputAccess.usageType == ResourceUsageType::Texture
                            ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                            : VK_IMAGE_LAYOUT_GENERAL;
                    const auto& imageView = *m_imageViews.at(res.physicalResourceIndex);
                    synchronizeImageAccess(
                        res, newLayout, inputAccess.stage, /*isWrite=*/false, imageView.getSubresourceRange());
                } else if (res.type == ResourceType::Buffer) {
                    const auto& physicalBuffer{m_physicalBuffers.at(res.physicalResourceIndex)};
                    ctx.commandEncoder.insertBufferMemoryBarrier(
                        *physicalBuffer.buffer, res.producerAccess.stage >> inputAccess.stage);
                }
            }
        };

    const auto cmdBuffer = encoder.getHandle();
    auto* gpuProfileFrame = m_passProfiler.beginFrame(frameContext.virtualFrameIndex);

    for (const auto&& [idx, pass] : std::views::enumerate(m_passes)) {
        if (gpuProfileFrame) {
            gpuProfileFrame->queryPool->writeTimestamp(
                cmdBuffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, static_cast<uint32_t>(idx) * 2);
        }

        // CRISP_LOGI("Executing pass: {}", pass.name);
        if (pass.type == PassType::Rasterizer) {
            synchronizeInputResources(pass, frameContext);

            std::vector<VkRenderingAttachmentInfo> colorAttachments;
            colorAttachments.reserve(pass.colorAttachments.size());
            uint32_t layerCount{0};
            const auto validateLayerCount = [this, &layerCount](const RenderGraphResourceHandle attachment) {
                const uint32_t attachmentLayerCount = getImageDescription(attachment).layerCount;
                if (layerCount == 0) {
                    layerCount = attachmentLayerCount;
                } else {
                    CRISP_CHECK_EQ(layerCount, attachmentLayerCount);
                }
            };

            for (const RenderGraphResourceHandle resourceId : pass.colorAttachments) {
                const auto& resource = getResource(resourceId);
                const auto& imageDescription = getImageDescription(resourceId);
                auto& imageView = *m_imageViews.at(resource.physicalResourceIndex);
                synchronizeImageAccess(
                    resource,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    resource.producerAccess.stage,
                    /*isWrite=*/true,
                    imageView.getSubresourceRange());
                colorAttachments.push_back(createRenderingAttachmentInfo(
                    resource, imageDescription, imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
                validateLayerCount(resourceId);
            }

            std::optional<VkRenderingAttachmentInfo> depthStencilAttachment;
            const VkRenderingAttachmentInfo* depthAttachment{nullptr};
            const VkRenderingAttachmentInfo* stencilAttachment{nullptr};
            if (pass.depthStencilAttachment) {
                const auto resourceId = *pass.depthStencilAttachment;
                const auto& resource = getResource(resourceId);
                const auto& imageDescription = getImageDescription(resourceId);
                auto& imageView = *m_imageViews.at(resource.physicalResourceIndex);
                synchronizeImageAccess(
                    resource,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    resource.producerAccess.stage,
                    /*isWrite=*/true,
                    imageView.getSubresourceRange());
                depthStencilAttachment = createRenderingAttachmentInfo(
                    resource, imageDescription, imageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

                const auto aspectFlags = determineImageAspect(imageDescription.format);
                depthAttachment = aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT ? &*depthStencilAttachment : nullptr;
                stencilAttachment = aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT ? &*depthStencilAttachment : nullptr;
                validateLayerCount(resourceId);
            }

            const VkExtent2D renderArea = getRenderArea(pass, m_swapChainExtent);
            CRISP_CHECK_GT(renderArea.width, 0);
            CRISP_CHECK_GT(renderArea.height, 0);
            CRISP_CHECK_GT(layerCount, 0);

            const VkRenderingInfo renderingInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {.offset = {0, 0}, .extent = renderArea},
                .layerCount = layerCount,
                .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
                .pColorAttachments = colorAttachments.empty() ? nullptr : colorAttachments.data(),
                .pDepthAttachment = depthAttachment,
                .pStencilAttachment = stencilAttachment,
            };

            encoder.beginRendering(renderingInfo);
            encoder.setViewport(
                {0.0f, 0.0f, static_cast<float>(renderArea.width), static_cast<float>(renderArea.height), 0.0f, 1.0f});
            encoder.setScissor({.offset = {0, 0}, .extent = renderArea});
            pass.executeFunc(frameContext);
            encoder.endRendering();
        } else if (pass.type == PassType::Compute || pass.type == PassType::RayTracing) {
            synchronizeInputResources(pass, frameContext);
            for (const RenderGraphResourceHandle resourceId : pass.outputs) {
                const auto& resource = getResource(resourceId);
                if (resource.type != ResourceType::Image) {
                    continue;
                }

                CRISP_CHECK_EQ(resource.producerAccess.usageType, ResourceUsageType::Storage);
                const auto& imageView = *m_imageViews.at(resource.physicalResourceIndex);
                synchronizeImageAccess(
                    resource,
                    VK_IMAGE_LAYOUT_GENERAL,
                    resource.producerAccess.stage,
                    /*isWrite=*/true,
                    imageView.getSubresourceRange());
            }
            pass.executeFunc(frameContext);
        }

        if (gpuProfileFrame) {
            gpuProfileFrame->queryPool->writeTimestamp(
                cmdBuffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, static_cast<uint32_t>(idx) * 2 + 1);
        }
    }

    for (const auto& resource : m_resources) {
        if (resource.type != ResourceType::Image || !resource.externalAccess) {
            continue;
        }

        const auto& imageView = *m_imageViews.at(resource.physicalResourceIndex);
        synchronizeImageAccess(
            resource,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            *resource.externalAccess,
            /*isWrite=*/false,
            imageView.getSubresourceRange());
    }

    m_passProfiler.endFrame(gpuProfileFrame);
}

void RenderGraph::PassProfiler::initialize(const VulkanDevice& vulkanDevice, const size_t passCount) {
    const uint32_t requiredQueryCount = static_cast<uint32_t>(passCount) * 2;
    const uint32_t timestampValidBits = vulkanDevice.getGeneralQueue().getTimestampValidBits();

    if (device != &vulkanDevice || queryCount != requiredQueryCount || timestampValidBits == 0) {
        frames.clear();
    }

    device = &vulkanDevice;
    queryCount = timestampValidBits == 0 ? 0 : requiredQueryCount;
    passTimingsMs.assign(passCount, std::nullopt);
    graphTimingMs.reset();

    for (auto& frame : frames) {
        frame.queryPool->reset();
        frame.pending = false;
    }
}

RenderGraph::PassProfiler::Frame* RenderGraph::PassProfiler::beginFrame(const uint32_t virtualFrameIndex) {
    if (!device || queryCount == 0) {
        return nullptr;
    }

    while (frames.size() <= virtualFrameIndex) {
        auto& frame = frames.emplace_back();
        frame.timestamps.resize(queryCount);
        frame.queryPool = std::make_unique<VulkanTimestampQueryPool>(
            *device, device->getGeneralQueue(), queryCount, fmt::format("RenderGraph GPU Queries {}", frames.size() - 1));
    }

    auto& frame = frames[virtualFrameIndex];
    if (!frame.pending) {
        return &frame;
    }

    // Renderer has already retired this virtual frame's previous submission before RenderGraph::execute().
    if (!frame.queryPool->tryGetResults(frame.timestamps)) {
        return nullptr;
    }

    for (size_t passIndex = 0; passIndex < passTimingsMs.size(); ++passIndex) {
        const uint64_t begin = frame.timestamps[passIndex * 2];
        const uint64_t end = frame.timestamps[passIndex * 2 + 1];
        passTimingsMs[passIndex] = frame.queryPool->getElapsedMilliseconds(begin, end);
    }
    graphTimingMs = frame.queryPool->getElapsedMilliseconds(frame.timestamps.front(), frame.timestamps.back());

    frame.queryPool->reset();
    frame.pending = false;
    return &frame;
}

RenderGraphBlackboard& RenderGraph::getBlackboard() {
    return m_blackboard;
}

VulkanRasterizationPassDescriptor RenderGraph::getRasterizationPassDescriptor(
    const RenderGraphPassHandle passHandle) const {
    const auto& pass = getPass(passHandle);
    CRISP_CHECK_EQ(pass.type, PassType::Rasterizer);

    VulkanRasterizationPassDescriptor descriptor{};
    bool hasAttachment{false};
    const auto addSampleCount = [&descriptor, &hasAttachment](const VkSampleCountFlagBits sampleCount) {
        if (hasAttachment) {
            CRISP_CHECK_EQ(descriptor.sampleCount, sampleCount);
        } else {
            descriptor.sampleCount = sampleCount;
            hasAttachment = true;
        }
    };

    descriptor.colorAttachmentFormats.reserve(pass.colorAttachments.size());
    for (const auto attachment : pass.colorAttachments) {
        const auto& imageDescription = getImageDescription(attachment);
        descriptor.colorAttachmentFormats.push_back(imageDescription.format);
        addSampleCount(imageDescription.sampleCount);
    }

    if (pass.depthStencilAttachment) {
        const auto& imageDescription = getImageDescription(*pass.depthStencilAttachment);
        const auto aspectFlags = determineImageAspect(imageDescription.format);
        if (aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT) {
            descriptor.depthAttachmentFormat = imageDescription.format;
        }
        if (aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT) {
            descriptor.stencilAttachmentFormat = imageDescription.format;
        }
        addSampleCount(imageDescription.sampleCount);
    }

    return descriptor;
}

VulkanRasterizationPassDescriptor RenderGraph::getRasterizationPassDescriptor(const std::string& name) const {
    return getRasterizationPassDescriptor(m_passMap.at(name));
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
        CRISP_LOGD(
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
    CRISP_LOGD("Determining resources to alias...");
    m_physicalBuffers.clear();
    m_physicalImages.clear();
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

    for (const auto& pass : m_passes) {
        for (const auto& [inputIndex, inputAccess] : std::views::enumerate(pass.inputAccesses)) {
            const auto& resource = getResource(pass.inputs[inputIndex]);
            if (resource.type != ResourceType::Image) {
                continue;
            }

            auto& physicalImage = m_physicalImages.at(resource.physicalResourceIndex);
            auto& readAccess =
                inputAccess.usageType == ResourceUsageType::Texture
                    ? physicalImage.shaderReadAccess
                    : physicalImage.generalReadAccess;
            readAccess = readAccess | inputAccess.stage;
        }
    }

    for (const auto& resource : m_resources) {
        if (resource.type == ResourceType::Image && resource.externalAccess) {
            auto& physicalImage = m_physicalImages.at(resource.physicalResourceIndex);
            physicalImage.shaderReadAccess = physicalImage.shaderReadAccess | *resource.externalAccess;
        }
    }

    CRISP_LOGD("{} physical buffer(s), {} physical image(s).", currPhysBufferIdx, currPhysImageIdx);
}

void RenderGraph::createPhysicalResources(
    const VulkanDevice& device, const VkExtent2D swapChainExtent, const VkCommandBuffer cmdBuffer) {
    CRISP_LOGD("Creating physical resources...");
    m_imageViews.clear();
    const VulkanCommandEncoder commandEncoder{cmdBuffer};
    for (auto& physicalImage : m_physicalImages) {
        const auto debugName =
            createPhysicalResourceDebugName("Image", m_resources, physicalImage.aliasedResourceIndices);
        const auto& desc = m_imageDescriptions[physicalImage.descriptionIndex];
        physicalImage.image = std::make_unique<VulkanImage>(
            device,
            VulkanImageDescription{
                .imageType = desc.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D,
                .format = desc.format,
                .sampleCount = desc.sampleCount,
                .extent = calculateImageExtent(desc, swapChainExtent),
                .mipLevelCount = desc.mipLevelCount,
                .layerCount = desc.layerCount,
                .usageFlags = determineUsageFlags(physicalImage.aliasedResourceIndices),
                .createFlags = determineCreateFlags(physicalImage.aliasedResourceIndices),
            });
        device.setObjectName(*physicalImage.image, debugName);
        device.setObjectName(physicalImage.image->getView(), fmt::format("{} Default View", debugName));
        CRISP_LOGT(
            "Created image: {} with size: {}x{}",
            debugName,
            physicalImage.image->getWidth(),
            physicalImage.image->getHeight());

        // TODO(fallenshard): Looks like a hack.
        const auto lastUsageFlags = getImageDescription({physicalImage.aliasedResourceIndices.back()}).imageUsageFlags;
        const auto [initialLayout, stage] = determineInitialLayout(physicalImage, lastUsageFlags);
        commandEncoder.transitionLayout(*physicalImage.image, initialLayout, kNullStage >> stage);
    }

    for (auto&& [physicalResourceIndex, physicalImage] : std::views::enumerate(m_physicalImages)) {
        auto& image = *physicalImage.image;
        const auto& desc = m_imageDescriptions[physicalImage.descriptionIndex];
        const auto imageType = desc.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
        auto view = createView(device, image, getImageViewType(imageType, image.getLayerCount(), false));
        const auto debugName =
            createPhysicalResourceDebugName("Image", m_resources, physicalImage.aliasedResourceIndices);
        device.setObjectName(*view, fmt::format("{} View", debugName));
        m_imageViews[static_cast<uint32_t>(physicalResourceIndex)] = std::move(view);
    }

    for (auto& res : m_physicalBuffers) {
        const auto& desc = m_bufferDescriptions[res.descriptionIndex];
        res.buffer = std::make_unique<VulkanBuffer>(device, desc.size, desc.usageFlags, BufferMemoryType::GpuOnly);
        device.setObjectName(
            *res.buffer, createPhysicalResourceDebugName("Buffer", m_resources, res.aliasedResourceIndices));
    }
}

} // namespace crisp::rg
