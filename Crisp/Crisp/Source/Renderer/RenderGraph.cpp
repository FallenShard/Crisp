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

    void RenderGraph::resize(int width, int height)
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
        RenderGraph::Node* sourceNode = m_nodes.at(sourcePass).get();
        VkImageAspectFlags attachmentAspect = sourceNode->renderPass->getRenderTarget(sourceRenderTargetIndex)->getAspectMask();
        if (attachmentAspect == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            sourceNode->dependencies[destinationPass] = [sourceRenderTargetIndex](const VulkanRenderPass& sourcePass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                auto renderTarget = sourcePass.getRenderTarget(sourceRenderTargetIndex);
                renderTarget->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex, 1,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            };
        }
        else if (attachmentAspect == VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            sourceNode->dependencies[destinationPass] = [sourceRenderTargetIndex](const VulkanRenderPass& sourcePass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
            {
                auto renderTarget = sourcePass.getRenderTarget(sourceRenderTargetIndex);
                renderTarget->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex, 1,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            };
        }
    }

    void RenderGraph::sortRenderPasses()
    {
        std::unordered_map<std::string, int> fanIn;
        for (auto& nodeEntry : m_nodes)
        {
            if (fanIn.count(nodeEntry.first) == 0)
                fanIn.emplace(nodeEntry.first, 0);

            for (auto& dependency : nodeEntry.second->dependencies)
                if (m_nodes.count(dependency.first) > 0)
                    fanIn[dependency.first]++;
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

    void RenderGraph::executeDrawCommands() const
    {
        m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer)
        {
            uint32_t frameIndex = m_renderer->getCurrentVirtualFrameIndex();
            for (auto node : m_executionOrder)
            {
                node->renderPass->begin(cmdBuffer);
                for (uint32_t subpassIndex = 0; subpassIndex < node->renderPass->getNumSubpasses(); subpassIndex++)
                {
                    if (subpassIndex > 0)
                        node->renderPass->nextSubpass(cmdBuffer);

                    for (const auto& command : node->commands[subpassIndex])
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

                        command.pipeline->getPipelineLayout()->setPushConstants(cmdBuffer, command.pushConstants.data());

                        for (uint32_t i = 0; i < command.dynamicBuffers.size(); ++i)
                        {
                            const auto& dynBuffer = command.dynamicBuffers[i];
                            command.material->setDynamicOffset(frameIndex, i, dynBuffer.buffer.getDynamicOffset(frameIndex) + dynBuffer.subOffset);
                        }

                        command.material->bind(frameIndex, cmdBuffer);
                        command.geometry->bindVertexBuffers(cmdBuffer);
                        command.drawFunc(cmdBuffer, command.geometryView);
                    }
                }

                node->renderPass->end(cmdBuffer);
                for (const auto& dep : node->dependencies)
                    dep.second(*node->renderPass, cmdBuffer, frameIndex);
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