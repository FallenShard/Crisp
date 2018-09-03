#include "RenderGraph.hpp"

#include "Vulkan/VulkanPipeline.hpp"
#include "Renderer/Material.hpp"
#include "Geometry/Geometry.hpp"

namespace crisp
{
    RenderGraph::RenderGraph(Renderer* renderer)
        : m_renderer(renderer)
    {
    }

    RenderGraph::~RenderGraph()
    {
    }

    RenderGraph::Node& RenderGraph::addRenderPass(std::unique_ptr<VulkanRenderPass> renderPass)
    {
        m_nodes.emplace_back(std::make_unique<Node>(std::move(renderPass)));
        return *m_nodes.back();
    }

    void RenderGraph::resize(int width, int height)
    {
        for (auto& node : m_nodes)
            node->renderPass->recreate();
    }

    void RenderGraph::executeDrawCommands() const
    {
        m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer)
        {
            uint32_t frameIndex = m_renderer->getCurrentVirtualFrameIndex();

            for (auto index : m_executionOrder)
            {
                const auto& node = m_nodes.at(index);
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
                            m_renderer->setDefaultViewport(cmdBuffer);
                        if (dynamicState & PipelineDynamicState::Scissor)
                            m_renderer->setDefaultScissor(cmdBuffer);

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
                    dep(*node->renderPass, cmdBuffer, frameIndex);
            }
        });
    }

    void RenderGraph::setExecutionOrder(std::vector<int>&& executionOrder)
    {
        m_executionOrder = executionOrder;
    }

    const RenderGraph::Node& RenderGraph::getNode(int index) const
    {
        return *m_nodes.at(index);
    }

    RenderGraph::Node::Node(std::unique_ptr<VulkanRenderPass> renderPass)
        : renderPass(std::move(renderPass))
    {
        commands.resize(this->renderPass->getNumSubpasses());
    }

    void RenderGraph::Node::addCommand(DrawCommand&& command, uint32_t subpassIndex)
    {
        commands.at(subpassIndex).emplace_back(command);
    }
}