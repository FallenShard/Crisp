#include <Crisp/Renderer/RenderGraph.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>

#include <stack>
#include <string_view>

namespace crisp {
namespace {
auto logger = createLoggerMt("RenderGraphOriginal");
} // namespace

RenderGraph::RenderGraph(Renderer* renderer)
    : m_renderer(renderer) {
    m_secondaryCommandBuffers.resize(kRendererVirtualFrameCount);
    for (auto& perFrameCtx : m_secondaryCommandBuffers) {
        perFrameCtx.resize(std::thread::hardware_concurrency());
        for (auto& ctx : perFrameCtx) {
            ctx.pool = std::make_unique<VulkanCommandPool>(
                m_renderer->getDevice().getGeneralQueue().createCommandPool(0),
                m_renderer->getDevice().getResourceDeallocator());
            ctx.cmdBuffer = std::make_unique<VulkanCommandBuffer>(
                ctx.pool->allocateCommandBuffer(m_renderer->getDevice(), VK_COMMAND_BUFFER_LEVEL_SECONDARY));
        }
    }
}

RenderGraph::Node& RenderGraph::addRenderPass(const std::string& name, std::unique_ptr<VulkanRenderPass> renderPass) {
    CRISP_CHECK(m_nodes.find(name) == m_nodes.end(), "Render graph already contains a node named {}", name);

    const auto& [iter, inserted] = m_nodes.emplace(name, std::make_unique<Node>(name, std::move(renderPass)));
    m_renderer->enqueueResourceUpdate([node = iter->second.get()](VkCommandBuffer cmdBuffer) {
        CRISP_LOGI("Initializing render target layouts for {}", node->name); // NOLINT
        // node->renderPass->updateInitialLayouts(cmdBuffer);
    });
    return *iter->second;
}

RenderGraph::Node& RenderGraph::addComputePass(const std::string& name) {
    CRISP_CHECK(m_nodes.find(name) == m_nodes.end(), "Render graph already contains a node named {}", name);

    const auto& [iter, inserted] = m_nodes.emplace(name, std::make_unique<Node>());
    iter->second->name = name;
    iter->second->type = NodeType::Compute;
    return *iter->second;
}

void RenderGraph::resize(int /*width*/, int /*height*/) {
    for (auto& [key, node] : m_nodes) {
        if (node->type != NodeType::Compute) {
            // node->renderPass->recreate(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
            // m_renderer->updateInitialLayouts(*node->renderPass);
        }
    }
}

void RenderGraph::addDependency(
    const std::string& srcPass, const std::string& dstPass, RenderGraph::DependencyCallback callback) {
    m_nodes.at(srcPass)->dependencies[dstPass] = std::move(callback);
}

void RenderGraph::addDependencyToPresentation(const std::string& srcPass, const uint32_t srcAttachmentIndex) {
    addDependency(srcPass, "PRESENT", srcAttachmentIndex);
}

void RenderGraph::addDependency(
    const std::string& srcPass, const std::string& dstPass, const uint32_t srcAttachmentIndex) {
    auto& sourceNode{*m_nodes.at(srcPass)};
    const auto* dstNode{m_nodes.contains(dstPass) ? m_nodes.at(dstPass).get() : nullptr};
    const VkPipelineStageFlagBits dstStageFlags{
        dstNode && dstNode->type == NodeType::Compute
            ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
            : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    // We take the attachment aspect to determine if we are transitioning depth or color target.
    const VkImageAspectFlags attachmentAspect{
        sourceNode.renderPass->getAttachmentView(srcAttachmentIndex).getSubresourceRange().aspectMask};
    const VkPipelineStageFlagBits srcStageFlags{
        attachmentAspect == VK_IMAGE_ASPECT_COLOR_BIT
            ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT};

    sourceNode.dependencies[dstPass] =
        [srcAttachmentIndex, srcStageFlags, dstStageFlags](
            const VulkanRenderPass& srcPass, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
            // auto& renderTarget{srcPass.getRenderTargetFromAttachmentIndex(srcAttachmentIndex)};
            // renderTarget.transitionLayout(
            //     cmdBuffer.getHandle(),
            //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            //     srcPass.getAttachmentView(srcAttachmentIndex).getSubresourceRange(),
            //     srcStageFlags,
            //     dstStageFlags);
        };
}

Result<> RenderGraph::sortRenderPasses() {
    HashMap<std::string, int32_t> fanIn;
    for (auto& [srcPass, node] : m_nodes) {
        if (fanIn.count(srcPass) == 0) {
            fanIn.emplace(srcPass, 0);
        }

        for (auto& [dstPass, callback] : node->dependencies) {
            if (m_nodes.count(dstPass) > 0) {
                fanIn[dstPass]++;
            }
        }
    }

    std::stack<std::string> stack;
    for (auto& entry : fanIn) {
        if (entry.second == 0) {
            stack.push(entry.first);
        }
    }

    m_executionOrder.clear();
    while (!stack.empty()) {
        auto node = m_nodes.at(stack.top()).get();
        stack.pop();

        m_executionOrder.push_back(node);
        for (auto& entry : node->dependencies) {
            if (--fanIn[entry.first] == 0) {
                stack.push(entry.first);
            }
        }
    }

    if (m_executionOrder.size() != m_nodes.size()) {
        return resultError("Render graph contains a cycle!");
    }

    return {};
}

void RenderGraph::printExecutionOrder() {
    for (uint32_t i = 0; i < m_executionOrder.size(); ++i) {
        CRISP_LOGI("{}. {}", i, m_executionOrder[i]->name);
    }
}

void RenderGraph::clearCommandLists() {
    for (auto& node : m_nodes) {
        for (auto& subpassCommandList : node.second->commands) {
            subpassCommandList.clear();
        }
    }
}

void RenderGraph::addToCommandLists(const RenderNode& renderNode) {
    if (!renderNode.isVisible) {
        return;
    }

    for (const auto& [key, materialMap] : renderNode.materials) {
        for (const auto& [part, material] : materialMap) {
            m_nodes.at(key.renderPassName)->addCommand(material.createDrawCommand(renderNode), key.subpassIndex);
        }
    }
}

void RenderGraph::buildCommandLists(const FlatHashMap<std::string, std::unique_ptr<RenderNode>>& renderNodes) {
    for (const auto& [id, renderNode] : renderNodes) {
        // CRISP_LOGI("Building commands for {}.", id);
        addToCommandLists(*renderNode);
    }
}

void RenderGraph::buildCommandLists(const std::vector<std::unique_ptr<RenderNode>>& renderNodes) {
    m_threadPool.parallelJob(renderNodes.size(), [this, &renderNodes](size_t start, size_t end, size_t /*jobIdx*/) {
        for (std::size_t k = start; k < end; ++k) {
            addToCommandLists(*renderNodes.at(k));
        }
    });
}

void RenderGraph::buildCommandLists(const FlatHashMap<std::string, RenderNode>& renderNodes) {
    for (const auto& [id, renderNode] : renderNodes) {
        addToCommandLists(renderNode);
    }
}

void RenderGraph::executeCommandLists() const {
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
        VulkanCommandBuffer commandBuffer(cmdBuffer);
        uint32_t frameIndex = m_renderer->getCurrentVirtualFrameIndex();
        for (auto node : m_executionOrder) {
            if (node->type == NodeType::Compute) {
                executeComputePass(commandBuffer, frameIndex, *node);
            } else {
                executeRenderPass(commandBuffer, frameIndex, *node);
            }
        }
    });
}

const RenderGraph::Node& RenderGraph::getNode(const std::string& name) const {
    return *m_nodes.at(name);
}

RenderGraph::Node& RenderGraph::getNode(const std::string& name) {
    return *m_nodes.at(name);
}

const VulkanRenderPass& RenderGraph::getRenderPass(const std::string& name) {
    return *m_nodes.at(name)->renderPass;
}

void RenderGraph::executeDrawCommand(
    const DrawCommand& command, const Renderer& renderer, const VulkanCommandBuffer& cmdBuffer) {
    command.pipeline->bind(cmdBuffer.getHandle());
    const auto dynamicState = command.pipeline->getDynamicStateFlags();
    if (dynamicState & PipelineDynamicState::Viewport) {
        if (command.viewport.width != 0.0f) {
            vkCmdSetViewport(cmdBuffer.getHandle(), 0, 1, &command.viewport);
        } else {
            renderer.setDefaultViewport(cmdBuffer.getHandle());
        }
    }

    if (dynamicState & PipelineDynamicState::Scissor) {
        if (command.scissor.extent.width != 0) {
            vkCmdSetScissor(cmdBuffer.getHandle(), 0, 1, &command.scissor);
        } else {
            renderer.setDefaultScissor(cmdBuffer.getHandle());
        }
    }

    command.pipeline->getPipelineLayout()->setPushConstants(
        cmdBuffer.getHandle(), static_cast<const char*>(command.pushConstantView.data));

    if (command.material) {
        command.material->bind(cmdBuffer.getHandle(), command.dynamicBufferOffsets);
    }

    command.geometry->bindVertexBuffers(cmdBuffer.getHandle(), command.firstBuffer, command.bufferCount);
    command.drawFunc(cmdBuffer.getHandle(), command.geometryView);
}

void RenderGraph::executeRenderPass(VulkanCommandBuffer& cmdBuffer, uint32_t virtualFrameIndex, const Node& node) const {
    bool useSecondaries = node.commands[0].size() > 100;

    // node.renderPass->begin(
    //     cmdBuffer.getHandle(),
    //     useSecondaries ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
    for (uint32_t subpassIndex = 0; subpassIndex < node.renderPass->getSubpassCount(); subpassIndex++) {
        if (subpassIndex > 0) {
            useSecondaries = node.commands[subpassIndex].size() > 100;
            node.renderPass->nextSubpass(
                cmdBuffer.getHandle(),
                useSecondaries ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
        }

        if (useSecondaries) {
            m_threadPool.parallelJob(
                node.commands[subpassIndex].size(),
                [this, &node, subpassIndex](size_t start, size_t end, size_t jobIdx) {
                    uint32_t frameIdx = m_renderer->getCurrentVirtualFrameIndex();
                    auto& cmdCtx = m_secondaryCommandBuffers[frameIdx][jobIdx];

                    VkCommandBufferInheritanceInfo inheritance = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
                    // inheritance.framebuffer = node.renderPass->getFramebuffer()->getHandle();
                    inheritance.renderPass = node.renderPass->getHandle();
                    inheritance.subpass = subpassIndex;
                    cmdCtx.cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance);

                    for (std::size_t k = start; k < end; ++k) {
                        executeDrawCommand(node.commands[subpassIndex][k], *m_renderer, *cmdCtx.cmdBuffer);
                    }

                    cmdCtx.cmdBuffer->end();
                });

            std::vector<VkCommandBuffer> secondaryBuffers;
            for (auto& cmdCtx : m_secondaryCommandBuffers[virtualFrameIndex]) {
                secondaryBuffers.push_back(cmdCtx.cmdBuffer->getHandle());
            }

            cmdBuffer.executeSecondaryBuffers(secondaryBuffers);
        } else {
            for (const auto& command : node.commands[subpassIndex]) {
                executeDrawCommand(command, *m_renderer, cmdBuffer);
            }
        }
    }

    node.renderPass->end(cmdBuffer.getHandle());
    for (const auto& dep : node.dependencies) {
        dep.second(*node.renderPass, cmdBuffer, virtualFrameIndex);
    }
}

void RenderGraph::executeComputePass(VulkanCommandBuffer& cmdBuffer, uint32_t virtualFrameIndex, Node& node) const {
    /*if (!node.isEnabled)
        return;*/

    node.pipeline->bind(cmdBuffer.getHandle());
    node.material->bind(cmdBuffer.getHandle());
    if (node.preDispatchCallback) {
        node.preDispatchCallback(node, cmdBuffer, virtualFrameIndex);
    }
    cmdBuffer.dispatchCompute(node.numWorkGroups);

    for (const auto& dep : node.dependencies) {
        dep.second(*node.renderPass, cmdBuffer, virtualFrameIndex);
    }
}

RenderGraph::Node::Node(std::string name, std::unique_ptr<VulkanRenderPass> renderPass)
    : name(std::move(name))
    , renderPass(std::move(renderPass)) {
    commands.resize(this->renderPass->getSubpassCount());
}

void RenderGraph::Node::addCommand(DrawCommand&& command, uint32_t subpassIndex) {
    commands.at(subpassIndex).emplace_back(command);
}
} // namespace crisp