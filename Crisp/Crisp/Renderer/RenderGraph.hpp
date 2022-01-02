#pragma once

#include <Crisp/Core/ThreadPool.hpp>
#include <Crisp/Renderer/DrawCommand.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <CrispCore/Result.hpp>
#include <CrispCore/RobinHood.hpp>

#include <tbb/concurrent_vector.h>

namespace crisp
{
class Renderer;

class RenderGraph
{
public:
    using DependencyCallback = std::function<void(const VulkanRenderPass&, VulkanCommandBuffer&, uint32_t)>;

    struct Node
    {
        std::string name;
        std::unordered_map<std::string, DependencyCallback> dependencies;

        std::unique_ptr<VulkanRenderPass> renderPass;

        // Rendered nodes can be culled/filtered down into commands
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

        std::function<void(VulkanCommandBuffer&, uint32_t)> preDispatchCallback;
        bool isEnabled = true;
    };

    RenderGraph(Renderer* renderer);
    ~RenderGraph();

    Node& addRenderPass(std::string renderPassName, std::unique_ptr<VulkanRenderPass> renderPass);
    Node& addComputePass(std::string computePassName);
    void addDependency(std::string sourcePass, std::string destinationPass, RenderGraph::DependencyCallback callback);
    void addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass,
        uint32_t sourceRenderTargetIndex);
    void addRenderTargetLayoutTransition(const std::string& sourcePass, const std::string& destinationPass,
        uint32_t sourceRenderTargetIndex, uint32_t layerMultiplier);

    void resize(int width, int height);

    [[nodiscard]] Result<> sortRenderPasses();
    void printExecutionOrder();

    void clearCommandLists();
    void addToCommandLists(const RenderNode& renderNode);
    void buildCommandLists(const robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>>& renderNodes);
    void buildCommandLists(const std::vector<std::unique_ptr<RenderNode>>& renderNodes);
    void executeCommandLists() const;

    const Node& getNode(std::string name) const;
    Node& getNode(std::string name);

    const VulkanRenderPass& getRenderPass(std::string name);

    static void executeDrawCommand(const DrawCommand& command, Renderer& renderer, VulkanCommandBuffer& cmdBuffer,
        uint32_t virtualFrameIndex);

private:
    void executeRenderPass(VulkanCommandBuffer& buffer, uint32_t virtualFrameIndex, const Node& node) const;
    void executeComputePass(VulkanCommandBuffer& buffer, uint32_t virtualFrameIndex, const Node& node) const;

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
} // namespace crisp