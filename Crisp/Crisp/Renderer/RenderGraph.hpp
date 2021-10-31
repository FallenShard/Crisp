#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>
#include "DrawCommand.hpp"
#include "RenderNode.hpp"

#include <Crisp/Core/ThreadPool.hpp>

#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_vector.h>
#include <robin_hood/robin_hood.h>

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
            std::unordered_map<std::string, DependencyCallback> dependencies;

            std::unique_ptr<VulkanRenderPass> renderPass;

            // Rendered nodes can be culled/filtered down into commands
            //std::vector<std::vector<DrawCommand>> commands;

            tbb::concurrent_vector<tbb::concurrent_vector<DrawCommand>> commands;


            Node() = default;
            Node(std::string name, std::unique_ptr<VulkanRenderPass> renderPass);
            Node(Node&& node) = default;
            Node(const Node& node) = delete;

            void addCommand(DrawCommand&& command, uint32_t subpassIndex = 0);

            bool isCompute = false;
            std::unique_ptr<VulkanPipeline> pipeline;
            std::unique_ptr<Material> material;
            glm::ivec3 workGroupSize;
            glm::ivec3 numWorkGroups;

            std::function<void(VkCommandBuffer, uint32_t)> preDispatchCallback;
            bool isEnabled = true;
        };

        RenderGraph(Renderer* renderer);
        ~RenderGraph();

        Node& addRenderPass(std::string renderPassName, std::unique_ptr<VulkanRenderPass> renderPass);
        Node& addComputePass(std::string computePassName);
        void addDependency(std::string sourcePass, std::string destinationPass, RenderGraph::DependencyCallback callback);
        void addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass, uint32_t sourceRenderTargetIndex);
        void addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass, uint32_t sourceRenderTargetIndex, uint32_t layerMultiplier);

        void resize(int width, int height);

        void sortRenderPasses();
        void printExecutionOrder();

        void clearCommandLists();
        void addToCommandLists(const RenderNode& renderNode);
        void buildCommandLists(const std::unordered_map<std::string, std::unique_ptr<RenderNode>>& renderNodes);
        void buildCommandLists(const std::vector<std::unique_ptr<RenderNode>>& renderNodes);
        void executeCommandLists() const;

        const Node& getNode(std::string name) const;
        Node& getNode(std::string name);

        const VulkanRenderPass& getRenderPass(std::string name);

        static void executeDrawCommand(const DrawCommand& command, Renderer* renderer, VkCommandBuffer cmdBuffer, int virtualFrameIndex);

    private:
        void executeRenderPass(VkCommandBuffer buffer, uint32_t virtualFrameIndex, const Node& node) const;
        void executeComputePass(VkCommandBuffer buffer, uint32_t virtualFrameIndex, const Node& node) const;

        Renderer* m_renderer;

        std::vector<Node*> m_executionOrder;
        robin_hood::unordered_map<std::string, std::unique_ptr<Node>> m_nodes;


        struct CmdBufferContext
        {
            std::unique_ptr<VulkanCommandPool> pool;
            std::unique_ptr<VulkanCommandBuffer> cmdBuffer;
        };

        mutable ThreadPool m_threadPool;
        std::mutex m_commandListMutex;
        std::vector<std::vector<CmdBufferContext>> m_secondaryCommandBuffers;
    };
}