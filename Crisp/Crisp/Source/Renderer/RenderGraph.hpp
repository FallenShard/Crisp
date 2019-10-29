#pragma once

#include "vulkan/VulkanRenderPass.hpp"
#include "DrawCommand.hpp"
#include "RenderNode.hpp"

namespace crisp
{
    class Renderer;

    class RenderGraph
    {
    public:
        using DependencyCallback = std::function<void(const VulkanRenderPass&, VkCommandBuffer, uint32_t)>;
        struct Node
        {
            std::string name;
            std::unique_ptr<VulkanRenderPass> renderPass;

            // Rendered nodes can be culled/filtered down into commands
            std::vector<RenderNode> renderNodes;
            std::vector<std::vector<DrawCommand>> commands;

            std::unordered_map<std::string, DependencyCallback> dependencies;

            Node(std::string name, std::unique_ptr<VulkanRenderPass> renderPass);
            Node(Node&& node) = default;
            Node(const Node& node) = delete;

            void addCommand(DrawCommand&& command, uint32_t subpassIndex = 0);
        };

        RenderGraph(Renderer* renderer);
        ~RenderGraph();

        Node& addRenderPass(std::string renderPassName, std::unique_ptr<VulkanRenderPass> renderPass);
        void addDependency(std::string sourcePass, std::string destinationPass, RenderGraph::DependencyCallback callback);
        void addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass, uint32_t sourceRenderTargetIndex);
        void addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass, uint32_t sourceRenderTargetIndex, uint32_t layerMultiplier);

        void resize(int width, int height);

        void sortRenderPasses();

        void clearCommandLists();
        void buildCommandLists();
        void executeCommandLists() const;

        const Node& getNode(std::string name) const;
        Node& getNode(std::string name);

    private:
        Renderer* m_renderer;

        std::vector<Node*> m_executionOrder;
        std::unordered_map<std::string, std::unique_ptr<Node>> m_nodes;
    };
}