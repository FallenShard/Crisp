#include "RenderGraph.hpp"

#include "Vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanPipeline.hpp"
#include "Renderer/Material.hpp"
#include "Geometry/Geometry.hpp"

#include <CrispCore/Log.hpp>

#include <stack>
#include <string_view>
#include <exception>

namespace crisp
{
    RenderGraph::RenderGraph(Renderer* renderer)
        : m_renderer(renderer)
    {
    }

    RenderGraph::~RenderGraph()
    {
    }

    RenderGraph::Node& RenderGraph::addRenderPass(std::string name, std::unique_ptr<VulkanRenderPass> renderPass)
    {
        if (m_nodes.count(name) > 0)
            logWarning("Render graph already contains a node named {}\n", name);

        auto iter = m_nodes.emplace(name, std::make_unique<Node>(name, std::move(renderPass)));
        return *iter.first->second;
    }

    RenderGraph::Node& RenderGraph::addComputePass(std::string computePassName)
    {
        if (m_nodes.count(computePassName) > 0)
            logWarning("Render graph already contains a node named {}\n", computePassName);

        auto iter = m_nodes.emplace(computePassName, std::make_unique<Node>());
        iter.first->second->name = computePassName;
        iter.first->second->isCompute = true;
        return *iter.first->second;
    }

    void RenderGraph::resize(int /*width*/, int /*height*/)
    {
        for (auto& entry : m_nodes)
            entry.second->renderPass->recreate();
    }

    void RenderGraph::addDependency(std::string sourcePass, std::string destinationPass, RenderGraph::DependencyCallback callback)
    {
        m_nodes.at(sourcePass)->dependencies[destinationPass] = callback;
    }

    void RenderGraph::addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass, uint32_t sourceRenderTargetIndex)
    {
        addRenderTargetLayoutTransition(sourcePass, destinationPass, sourceRenderTargetIndex, 1);
    }

    void RenderGraph::addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass, uint32_t sourceRenderTargetIndex, uint32_t layerMultiplier)
    {
        RenderGraph::Node* sourceNode = m_nodes.at(sourcePass).get();
        VkImageAspectFlags attachmentAspect = sourceNode->renderPass->getRenderTarget(sourceRenderTargetIndex)->getAspectMask();
        RenderGraph::Node* dstNode = nullptr;
        if (m_nodes.count(destinationPass))
            dstNode = m_nodes.at(destinationPass).get();
        VkPipelineStageFlags dstStageFlags = dstNode && dstNode->isCompute ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        if (attachmentAspect == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            sourceNode->dependencies[destinationPass] = [sourceRenderTargetIndex, layerMultiplier, dstStageFlags](const VulkanRenderPass& sourcePass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                auto renderTarget = sourcePass.getRenderTarget(sourceRenderTargetIndex);
                renderTarget->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex * layerMultiplier, layerMultiplier,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, dstStageFlags);
            };
        }
        else if (attachmentAspect == VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            sourceNode->dependencies[destinationPass] = [sourceRenderTargetIndex, layerMultiplier, dstStageFlags](const VulkanRenderPass& sourcePass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                auto renderTarget = sourcePass.getRenderTarget(sourceRenderTargetIndex);
                renderTarget->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex * layerMultiplier, layerMultiplier,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, dstStageFlags);
            };
        }
    }

    void RenderGraph::sortRenderPasses()
    {
        std::unordered_map<std::string, int> fanIn;
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
            throw std::runtime_error("Render graph contains a cycle!");
    }

    void RenderGraph::clearCommandLists()
    {
        for (auto& node : m_nodes)
            for (auto& subpassCommandList : node.second->commands)
                subpassCommandList.clear();
    }

    void RenderGraph::buildCommandLists()
    {
        for (auto& graphNode : m_nodes)
        {
            auto& renderPass = graphNode.second;
            for (const auto& renderNode : renderPass->renderNodes)
                renderPass->addCommand(renderNode.createDrawCommand(m_renderer->getCurrentVirtualFrameIndex()), renderNode.subpassIndex);
        }
    }

    void RenderGraph::executeCommandLists() const
    {
        m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer)
        {
            uint32_t frameIndex = m_renderer->getCurrentVirtualFrameIndex();
            for (auto node : m_executionOrder)
            {
                if (node->isCompute)
                    executeComputePass(cmdBuffer, frameIndex, *node);
                else
                    executeRenderPass(cmdBuffer, frameIndex, *node);
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

    void RenderGraph::executeRenderPass(VkCommandBuffer cmdBuffer, uint32_t virtualFrameIndex, const Node& node) const
    {
        node.renderPass->begin(cmdBuffer);
        for (uint32_t subpassIndex = 0; subpassIndex < node.renderPass->getNumSubpasses(); subpassIndex++)
        {
            if (subpassIndex > 0)
                node.renderPass->nextSubpass(cmdBuffer);

            for (const auto& command : node.commands[subpassIndex])
            {
                command.pipeline->bind(cmdBuffer);
                auto dynamicState = command.pipeline->getDynamicStateFlags();
                if (dynamicState & PipelineDynamicState::Viewport)
                {
                    if (command.viewport.width != 0.0f)
                        vkCmdSetViewport(cmdBuffer, 0, 1, &command.viewport);
                    else
                        m_renderer->setDefaultViewport(cmdBuffer);
                }

                if (dynamicState & PipelineDynamicState::Scissor)
                {
                    if (command.scissor.extent.width != 0)
                        vkCmdSetScissor(cmdBuffer, 0, 1, &command.scissor);
                    else
                        m_renderer->setDefaultScissor(cmdBuffer);
                }

                command.pipeline->getPipelineLayout()->setPushConstants(cmdBuffer, static_cast<const char*>(command.pushConstantView.data));

                if (command.dynamicBufferOffsets.empty())
                {
                    for (uint32_t i = 0; i < command.dynamicBufferViews.size(); ++i)
                    {
                        const auto& view = command.dynamicBufferViews[i];
                        const uint32_t dynamicOffset = view.buffer->getDynamicOffset(virtualFrameIndex) + view.subOffset;
                        command.material->setDynamicOffset(virtualFrameIndex, i, dynamicOffset);
                    }

                    command.material->bind(virtualFrameIndex, cmdBuffer);
                }
                else
                {
                    command.material->bind(virtualFrameIndex, cmdBuffer, command.dynamicBufferOffsets);
                }

                command.geometry->bindVertexBuffers(cmdBuffer);
                command.drawFunc(cmdBuffer, command.geometryView);
            }
        }

        node.renderPass->end(cmdBuffer, virtualFrameIndex);
        for (const auto& dep : node.dependencies)
            dep.second(*node.renderPass, cmdBuffer, virtualFrameIndex);
    }

    void RenderGraph::executeComputePass(VkCommandBuffer cmdBuffer, uint32_t virtualFrameIndex, const Node& node) const
    {
        if (node.callback)
            node.callback(cmdBuffer, virtualFrameIndex);
        node.pipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        node.material->bind(virtualFrameIndex, cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        vkCmdDispatch(cmdBuffer, node.numWorkGroups.x, node.numWorkGroups.y, node.numWorkGroups.z);
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
}