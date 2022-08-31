#include "RenderGraph.hpp"

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <Crisp/Vulkan/VulkanFramebuffer.hpp>

#include <Crisp/Common/Logger.hpp>

#include <exception>
#include <stack>
#include <string_view>

#include "Crisp/ChromeProfiler.hpp"

#include <Crisp/Enumerate.hpp>

namespace crisp
{
namespace
{
auto logger = createLoggerMt("RenderGraph");
}

RenderGraph::RenderGraph(Renderer* renderer)
    : m_renderer(renderer)
{
    m_secondaryCommandBuffers.resize(RendererConfig::VirtualFrameCount);
    for (auto& perFrameCtx : m_secondaryCommandBuffers)
    {
        perFrameCtx.resize(std::thread::hardware_concurrency());
        for (auto& ctx : perFrameCtx)
        {
            ctx.pool = std::make_unique<VulkanCommandPool>(
                m_renderer->getDevice().getGeneralQueue().createCommandPool(
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
                m_renderer->getDevice().getResourceDeallocator());
            ctx.cmdBuffer = std::make_unique<VulkanCommandBuffer>(
                ctx.pool->allocateCommandBuffer(m_renderer->getDevice(), VK_COMMAND_BUFFER_LEVEL_SECONDARY));
        }
    }
}

RenderGraph::~RenderGraph() {}

RenderGraph::Node& RenderGraph::addRenderPass(std::string name, std::unique_ptr<VulkanRenderPass> renderPass)
{
    if (m_nodes.count(name) > 0)
        logger->warn("Render graph already contains a node named {}\n", name);

    auto iter = m_nodes.emplace(name, std::make_unique<Node>(name, std::move(renderPass)));
    // logger->info("Preparing render pass: {}.", iter.)
    m_renderer->enqueueResourceUpdate(
        [pass = iter](VkCommandBuffer cmdBuffer)
        {
            logger->info("Updating render pass {}", pass.first->first);
            pass.first->second->renderPass->updateInitialLayouts(cmdBuffer);
        });
    // updateInitialLayouts(*iter.first->second->renderPass);
    return *iter.first->second;
}

RenderGraph::Node& RenderGraph::addComputePass(std::string computePassName)
{
    if (m_nodes.count(computePassName) > 0)
        logger->warn("Render graph already contains a node named {}\n", computePassName);

    auto iter = m_nodes.emplace(computePassName, std::make_unique<Node>());
    iter.first->second->name = computePassName;
    iter.first->second->isCompute = true;
    return *iter.first->second;
}

void RenderGraph::resize(int /*width*/, int /*height*/)
{
    for (auto& [key, node] : m_nodes)
    {
        if (!node->isCompute)
        {
            node->renderPass->recreate(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
            m_renderer->updateInitialLayouts(*node->renderPass);
        }
    }
}

void RenderGraph::addDependency(
    std::string sourcePass, std::string destinationPass, RenderGraph::DependencyCallback callback)
{
    m_nodes.at(sourcePass)->dependencies[destinationPass] = callback;
}

void RenderGraph::addRenderTargetLayoutTransitionToScreen(
    const std::string& sourcePass, uint32_t sourceRenderTargetIndex)
{
    addRenderTargetLayoutTransition(sourcePass, "SCREEN", sourceRenderTargetIndex);
}

void RenderGraph::addRenderTargetLayoutTransition(
    const std::string& sourcePass, const std::string& destinationPass, uint32_t sourceRenderTargetIndex)
{
    addRenderTargetLayoutTransition(sourcePass, destinationPass, sourceRenderTargetIndex, 1);
}

void RenderGraph::addRenderTargetLayoutTransition(
    const std::string& sourcePass,
    const std::string& destinationPass,
    uint32_t sourceRenderTargetIndex,
    uint32_t layerMultiplier)
{
    RenderGraph::Node* sourceNode = m_nodes.at(sourcePass).get();
    VkImageAspectFlags attachmentAspect =
        sourceNode->renderPass->getRenderTarget(sourceRenderTargetIndex).getAspectMask();
    RenderGraph::Node* dstNode = nullptr;
    if (m_nodes.count(destinationPass))
        dstNode = m_nodes.at(destinationPass).get();
    VkPipelineStageFlags dstStageFlags =
        dstNode && dstNode->isCompute ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if (attachmentAspect == VK_IMAGE_ASPECT_COLOR_BIT)
    {
        sourceNode->dependencies[destinationPass] =
            [s = sourcePass, sourceRenderTargetIndex, layerMultiplier, dstStageFlags](
                const VulkanRenderPass& sourcePass, VulkanCommandBuffer& cmdBuffer, uint32_t frameIndex)
        {
            logger->info("Transitioning: {}", s);
            auto& renderTarget = sourcePass.getRenderTarget(sourceRenderTargetIndex);
            const auto& range =
                sourcePass.getRenderTargetView(sourceRenderTargetIndex, frameIndex).getSubresourceRange();

            renderTarget.transitionLayout(
                cmdBuffer.getHandle(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                range,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                dstStageFlags);
        };
    }
    else if (attachmentAspect == VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        sourceNode->dependencies[destinationPass] =
            [s = sourcePass, sourceRenderTargetIndex, layerMultiplier, dstStageFlags](
                const VulkanRenderPass& sourcePass, VulkanCommandBuffer& cmdBuffer, uint32_t frameIndex)
        {
            logger->info("Transitioning: {}", s);
            auto& renderTarget = sourcePass.getRenderTarget(sourceRenderTargetIndex);
            const auto& range =
                sourcePass.getRenderTargetView(sourceRenderTargetIndex, frameIndex).getSubresourceRange();
            renderTarget.transitionLayout(
                cmdBuffer.getHandle(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                range,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                dstStageFlags);
        };
    }
}

Result<> RenderGraph::sortRenderPasses()
{
    robin_hood::unordered_map<std::string, int32_t> fanIn;
    for (auto& [srcPass, node] : m_nodes)
    {
        if (fanIn.count(srcPass) == 0)
            fanIn.emplace(srcPass, 0);

        for (auto& [dstPass, callback] : node->dependencies)
            if (m_nodes.count(dstPass) > 0)
                fanIn[dstPass]++;
    }

    std::stack<std::string> stack;
    for (auto& entry : fanIn)
        if (entry.second == 0)
            stack.push(entry.first);

    m_executionOrder.clear();
    while (!stack.empty())
    {
        auto node = m_nodes.at(stack.top()).get();
        stack.pop();

        m_executionOrder.push_back(node);
        for (auto& entry : node->dependencies)
        {
            if (--fanIn[entry.first] == 0)
                stack.push(entry.first);
        }
    }

    if (m_executionOrder.size() != m_nodes.size())
        return resultError("Render graph contains a cycle!");

    return {};
}

void RenderGraph::printExecutionOrder()
{
    for (uint32_t i = 0; i < m_executionOrder.size(); ++i)
    {
        logger->warn("{}. {}", i, m_executionOrder[i]->name);
    }
}

void RenderGraph::clearCommandLists()
{
    for (auto& node : m_nodes)
        for (auto& subpassCommandList : node.second->commands)
            subpassCommandList.clear();
}

void RenderGraph::addToCommandLists(const RenderNode& renderNode)
{
    if (!renderNode.isVisible)
        return;

    const uint32_t virtualFrameIndex = m_renderer->getCurrentVirtualFrameIndex();
    for (const auto& [key, materialMap] : renderNode.materials)
        for (const auto& [part, material] : materialMap)
            m_nodes.at(key.renderPassName)
                ->addCommand(material.createDrawCommand(virtualFrameIndex, renderNode), key.subpassIndex);
}

void RenderGraph::buildCommandLists(
    const robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>>& renderNodes)
{
    for (const auto& [id, renderNode] : renderNodes)
        addToCommandLists(*renderNode);
}

void RenderGraph::buildCommandLists(const std::vector<std::unique_ptr<RenderNode>>& renderNodes)
{
    m_threadPool.parallelJob(
        renderNodes.size(),
        [this, &renderNodes](size_t start, size_t end, size_t /*jobIdx*/)
        {
            for (std::size_t k = start; k < end; ++k)
                addToCommandLists(*renderNodes.at(k));
        });
}

void RenderGraph::executeCommandLists() const
{
    m_renderer->enqueueDrawCommand(
        [this](VkCommandBuffer cmdBuffer)
        {
            VulkanCommandBuffer commandBuffer(cmdBuffer);
            uint32_t frameIndex = m_renderer->getCurrentVirtualFrameIndex();
            for (auto node : m_executionOrder)
            {
                if (node->isCompute)
                    executeComputePass(commandBuffer, frameIndex, *node);
                else
                    executeRenderPass(commandBuffer, frameIndex, *node);
            }
        });
}

const RenderGraph::Node& RenderGraph::getNode(std::string name) const
{
    return *m_nodes.at(name);
}

RenderGraph::Node& RenderGraph::getNode(std::string name)
{
    return *m_nodes.at(name);
}

const VulkanRenderPass& RenderGraph::getRenderPass(std::string name)
{
    return *m_nodes.at(name)->renderPass;
}

void RenderGraph::executeDrawCommand(
    const DrawCommand& command, Renderer& renderer, VulkanCommandBuffer& cmdBuffer, uint32_t virtualFrameIndex)
{
    command.pipeline->bind(cmdBuffer.getHandle());
    const auto dynamicState = command.pipeline->getDynamicStateFlags();
    if (dynamicState & PipelineDynamicState::Viewport)
    {
        if (command.viewport.width != 0.0f)
            vkCmdSetViewport(cmdBuffer.getHandle(), 0, 1, &command.viewport);
        else
            renderer.setDefaultViewport(cmdBuffer.getHandle());
    }

    if (dynamicState & PipelineDynamicState::Scissor)
    {
        if (command.scissor.extent.width != 0)
            vkCmdSetScissor(cmdBuffer.getHandle(), 0, 1, &command.scissor);
        else
            renderer.setDefaultScissor(cmdBuffer.getHandle());
    }

    command.pipeline->getPipelineLayout()->setPushConstants(
        cmdBuffer.getHandle(), static_cast<const char*>(command.pushConstantView.data));

    if (command.material)
        command.material->bind(virtualFrameIndex, cmdBuffer.getHandle(), command.dynamicBufferOffsets);

    command.geometry->bindVertexBuffers(cmdBuffer.getHandle());
    command.drawFunc(cmdBuffer.getHandle(), command.geometryView);
}

void RenderGraph::executeRenderPass(VulkanCommandBuffer& cmdBuffer, uint32_t virtualFrameIndex, const Node& node) const
{
    bool useSecondaries = node.commands[0].size() > 100;

    node.renderPass->begin(
        cmdBuffer.getHandle(),
        virtualFrameIndex,
        useSecondaries ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
    for (uint32_t subpassIndex = 0; subpassIndex < node.renderPass->getNumSubpasses(); subpassIndex++)
    {
        if (subpassIndex > 0)
        {
            useSecondaries = node.commands[subpassIndex].size() > 100;
            node.renderPass->nextSubpass(
                cmdBuffer.getHandle(),
                useSecondaries ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
        }

        if (useSecondaries)
        {
            m_threadPool.parallelJob(
                node.commands[subpassIndex].size(),
                [this, &node, subpassIndex](size_t start, size_t end, size_t jobIdx)
                {
                    uint32_t frameIdx = m_renderer->getCurrentVirtualFrameIndex();
                    auto& cmdCtx = m_secondaryCommandBuffers[frameIdx][jobIdx];

                    VkCommandBufferInheritanceInfo inheritance = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
                    inheritance.framebuffer = node.renderPass->getFramebuffer(frameIdx)->getHandle();
                    inheritance.renderPass = node.renderPass->getHandle();
                    inheritance.subpass = subpassIndex;
                    cmdCtx.cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance);

                    for (std::size_t k = start; k < end; ++k)
                        executeDrawCommand(node.commands[subpassIndex][k], *m_renderer, *cmdCtx.cmdBuffer, frameIdx);

                    cmdCtx.cmdBuffer->end();
                });

            std::vector<VkCommandBuffer> secondaryBuffers;
            for (auto& cmdCtx : m_secondaryCommandBuffers[virtualFrameIndex])
                secondaryBuffers.push_back(cmdCtx.cmdBuffer->getHandle());

            cmdBuffer.executeSecondaryBuffers(secondaryBuffers);
        }
        else
        {
            for (const auto& command : node.commands[subpassIndex])
                executeDrawCommand(command, *m_renderer, cmdBuffer, virtualFrameIndex);
        }
    }

    node.renderPass->end(cmdBuffer.getHandle(), virtualFrameIndex);
    for (const auto& dep : node.dependencies)
        dep.second(*node.renderPass, cmdBuffer, virtualFrameIndex);
}

void RenderGraph::executeComputePass(VulkanCommandBuffer& cmdBuffer, uint32_t virtualFrameIndex, Node& node) const
{
    /*if (!node.isEnabled)
        return;*/

    node.pipeline->bind(cmdBuffer.getHandle());
    node.material->bind(virtualFrameIndex, cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE);
    if (node.preDispatchCallback)
        node.preDispatchCallback(node, cmdBuffer, virtualFrameIndex);
    cmdBuffer.dispatchCompute(node.numWorkGroups);

    for (const auto& dep : node.dependencies)
        dep.second(*node.renderPass, cmdBuffer, virtualFrameIndex);
}

RenderGraph::Node::Node(std::string name, std::unique_ptr<VulkanRenderPass> renderPass)
    : name(name)
    , renderPass(std::move(renderPass))
{
    commands.resize(this->renderPass->getNumSubpasses());
}

void RenderGraph::Node::addCommand(DrawCommand&& command, uint32_t subpassIndex)
{
    commands.at(subpassIndex).emplace_back(command);
}
} // namespace crisp