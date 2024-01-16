#pragma once

#include <Crisp/Core/ThreadPool.hpp>
#include <Crisp/Renderer/DrawCommand.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Result.hpp>

#include <tbb/concurrent_vector.h>

namespace crisp {
class RenderGraph {
public:
    using DependencyCallback = std::function<void(const VulkanRenderPass&, VulkanCommandBuffer&, uint32_t)>;

    enum class NodeType { Rasterizer, Compute, Raytracing };

    struct Node {
        Node() = default;
        Node(std::string name, std::unique_ptr<VulkanRenderPass> renderPass);
        Node(Node&& node) = default;
        Node(const Node& node) = delete;

        void addCommand(DrawCommand&& command, uint32_t subpassIndex = 0);

        // General data.
        std::string name;
        NodeType type{NodeType::Rasterizer};
        HashMap<std::string, DependencyCallback> dependencies;
        bool isEnabled{true};

        // Rasterizer data.
        std::unique_ptr<VulkanRenderPass> renderPass;
        tbb::concurrent_vector<tbb::concurrent_vector<DrawCommand>> commands;

        // Compute data.
        std::unique_ptr<VulkanPipeline> pipeline;
        std::unique_ptr<Material> material;
        glm::ivec3 workGroupSize;
        glm::ivec3 numWorkGroups;
        std::function<void(Node&, VulkanCommandBuffer&, uint32_t)> preDispatchCallback;
    };

    RenderGraph(Renderer* renderer);

    Node& addRenderPass(const std::string& renderPassName, std::unique_ptr<VulkanRenderPass> renderPass);
    Node& addComputePass(const std::string& computePassName);

    void addDependency(const std::string& srcPass, const std::string& dstPass, DependencyCallback callback);
    void addDependencyToPresentation(const std::string& srcPass, uint32_t srcAttachmentIndex);
    void addDependency(const std::string& srcPass, const std::string& dstPass, uint32_t srcAttachmentIndex);

    void resize(int width, int height);

    [[nodiscard]] Result<> sortRenderPasses();
    void printExecutionOrder();

    void clearCommandLists();
    void addToCommandLists(const RenderNode& renderNode);
    void buildCommandLists(const FlatHashMap<std::string, std::unique_ptr<RenderNode>>& renderNodes);
    void buildCommandLists(const FlatHashMap<std::string, RenderNode>& renderNodes);
    void buildCommandLists(const std::vector<std::unique_ptr<RenderNode>>& renderNodes);
    void executeCommandLists() const;

    const Node& getNode(std::string name) const;
    Node& getNode(std::string name);

    const VulkanRenderPass& getRenderPass(std::string name);

    static void executeDrawCommand(
        const DrawCommand& command,
        const Renderer& renderer,
        const VulkanCommandBuffer& cmdBuffer,
        uint32_t virtualFrameIndex);

private:
    void executeRenderPass(VulkanCommandBuffer& buffer, uint32_t virtualFrameIndex, const Node& node) const;
    void executeComputePass(VulkanCommandBuffer& buffer, uint32_t virtualFrameIndex, Node& node) const;

    Renderer* m_renderer;

    std::vector<Node*> m_executionOrder;
    FlatHashMap<std::string, std::unique_ptr<Node>> m_nodes;

    struct CmdBufferContext {
        std::unique_ptr<VulkanCommandPool> pool;
        std::unique_ptr<VulkanCommandBuffer> cmdBuffer;
    };

    mutable ThreadPool m_threadPool;
    std::mutex m_commandListMutex;
    std::vector<std::vector<CmdBufferContext>> m_secondaryCommandBuffers;
};
} // namespace crisp