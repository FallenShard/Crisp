#pragma once

#include "vulkan/VulkanRenderPass.hpp"
#include "DrawCommand.hpp"

namespace crisp
{
    class Renderer;

    class RenderGraph
    {
    public:
        struct Node
        {
            std::unique_ptr<VulkanRenderPass> renderPass;
            std::vector<std::vector<DrawCommand>> commands;
            std::vector<std::function<void(const VulkanRenderPass&, VkCommandBuffer, uint32_t)>> dependencies;

            Node(std::unique_ptr<VulkanRenderPass> renderPass);
            Node(Node&& node) = default;
            Node(const Node& node) = delete;

            void addCommand(DrawCommand&& command, uint32_t subpassIndex = 0);
        };

        RenderGraph(Renderer* renderer);
        ~RenderGraph();

        Node& addRenderPass(std::unique_ptr<VulkanRenderPass> renderPass);

        void resize(int width, int height);

        void executeDrawCommands() const;

        void setExecutionOrder(std::vector<int>&& executionOrder);

        const Node& getNode(int index) const;

    private:
        Renderer* m_renderer;

        std::vector<std::unique_ptr<Node>> m_nodes;
        std::vector<int> m_executionOrder;
    };
}