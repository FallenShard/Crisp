#include <Crisp/Vulkan/Test/VulkanTest.hpp>

#include <stack>

namespace crisp::test
{
namespace
{

using ::testing::SizeIs;

enum class ResourceType
{
    Buffer,
    Image,
};

struct ResourceHandle
{
    uint32_t id;
};

struct PassHandle
{
    uint32_t id;
};

struct RGResource
{
    ResourceType type;
    uint32_t version{0};

    uint32_t size;

    PassHandle producerPass{~0u};
    ResourceHandle outputResource;

    int32_t refCount = 0;

    std::string name;
};

struct RGPass
{
    uint32_t technique;

    std::vector<ResourceHandle> inputs;
    std::vector<ResourceHandle> outputs;

    std::vector<PassHandle> edges;

    std::string name;
};

struct RenderGraph
{
    std::map<std::string, ResourceHandle> resourceMap;
    std::vector<RGResource> resources;
    std::vector<RGPass> passes;
};

ResourceHandle addImageResource(RenderGraph& rg, uint32_t size = 400)
{
    static uint32_t nextId = 0;
    rg.resources.push_back({ResourceType::Image, nextId++, size});
    return {static_cast<uint32_t>(rg.resources.size()) - 1};
}

ResourceHandle addBufferResource(RenderGraph& rg, uint32_t size = 100)
{
    static uint32_t nextId = 0;
    rg.resources.push_back({ResourceType::Buffer, nextId++, size});
    return {static_cast<uint32_t>(rg.resources.size()) - 1};
}

PassHandle addRenderPass(RenderGraph& rg, std::string name)
{
    static uint32_t nextId = 0;
    rg.passes.emplace_back();
    rg.passes.back().name = std::move(name);
    return {nextId++};
}

ResourceHandle declareOutput(RenderGraph& rg, PassHandle pass, ResourceHandle res)
{
    rg.resources[res.id].producerPass = pass;
    rg.passes[pass.id].outputs.push_back(res);
    return res;
}

ResourceHandle declareInputOutput(RenderGraph& rg, PassHandle pass, ResourceHandle res)
{
    auto modifiedResource = addImageResource(rg);
    rg.resources[modifiedResource.id] = rg.resources[res.id];
    rg.resources[modifiedResource.id].version++;

    rg.resources[modifiedResource.id].producerPass = pass;
    rg.passes[pass.id].outputs.push_back(modifiedResource);

    return modifiedResource;
}

void declareInput(RenderGraph& rg, PassHandle pass, ResourceHandle res)
{
    rg.passes[pass.id].inputs.push_back(res);
}

std::vector<std::string> topoSort(RenderGraph& graph)
{
    const auto nodeCount = graph.passes.size();
    for (uint32_t i = 0; i < nodeCount; ++i)
    {
        auto& pass = graph.passes[i];
        pass.edges.clear();
    }

    std::vector<int32_t> fanIn(nodeCount, 0);
    for (uint32_t i = 0; i < nodeCount; ++i)
    {
        auto& pass = graph.passes[i];
        for (const auto& input : pass.inputs)
        {
            const auto& res = graph.resources[input.id];
            if (res.producerPass.id != ~0u)
            {
                fanIn[i]++;
                graph.passes[res.producerPass.id].edges.push_back({i});
                fmt::print("Adding edge from {} to {}\n", graph.passes[res.producerPass.id].name, pass.name);
            }
        }
    }

    std::stack<uint32_t> st;
    for (uint32_t i = 0; i < nodeCount; ++i)
    {
        if (fanIn[i] == 0)
        {
            st.push(i);
        }
    }

    std::vector<std::string> execOrder;
    while (!st.empty())
    {
        auto node = st.top();
        st.pop();

        execOrder.push_back(graph.passes[node].name);
        for (auto nbr : graph.passes[node].edges)
        {
            if (--fanIn[nbr.id] == 0)
            {
                st.push(nbr.id);
            }
        }
    }

    std::cout << "Done" << std::endl;

    return execOrder;
}

TEST(RenderGraphTest, BasicUsage)
{
    RenderGraph rg;
    auto fluidPass = addRenderPass(rg, "fluidPass");
    const auto b0 = declareOutput(rg, fluidPass, addBufferResource(rg));

    auto depthPrePass = addRenderPass(rg, "depthPrePass");
    const auto depthBuffer = declareOutput(rg, depthPrePass, addImageResource(rg));
    declareInput(rg, depthPrePass, b0);

    auto forwardPass = addRenderPass(rg, "forwardPass");
    declareInput(rg, forwardPass, depthBuffer);
    declareInput(rg, forwardPass, b0);
    const auto hdrImage = declareOutput(rg, forwardPass, addImageResource(rg));

    auto transparentPass = addRenderPass(rg, "transparentPass");
    declareInput(rg, transparentPass, hdrImage);
    declareInput(rg, transparentPass, depthBuffer);
    auto modifiedHdrImage = declareInputOutput(rg, transparentPass, hdrImage);

    auto bloomPass = addRenderPass(rg, "bloomPass");
    declareInput(rg, bloomPass, modifiedHdrImage);
    const auto bloomImage = declareOutput(rg, bloomPass, addImageResource(rg));

    auto postprocessingPass = addRenderPass(rg, "tonemappingPass");
    declareInput(rg, postprocessingPass, bloomImage);
    const auto ldrImage = declareOutput(rg, bloomPass, addImageResource(rg));

    const auto order = topoSort(rg);

    for (const auto o : order)
    {
        std::cout << o << std::endl;
    }

    EXPECT_THAT(rg.passes, SizeIs(6));
    EXPECT_THAT(rg.resources, SizeIs(6));
}
} // namespace
} // namespace crisp::test