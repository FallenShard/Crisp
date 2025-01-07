#pragma once

#include <Crisp/Core/ConnectionHandler.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>

namespace crisp {
struct UpdateParams {
    uint32_t frameIdx;
    uint32_t frameInFlightIdx;
    float dt;
    float totalTimeSec;
};

class Scene {
public:
    Scene(Renderer* renderer, Window* window);
    virtual ~Scene() = default;

    virtual void resize(int width, int height) = 0;
    virtual void update(const UpdateParams& updateParams) = 0;
    virtual void render() = 0;

    virtual void renderGui() {}

protected:
    Window* m_window{nullptr};
    Renderer* m_renderer{nullptr};

    std::vector<ConnectionHandler> m_connectionHandlers;

    std::unique_ptr<ResourceContext> m_resourceContext;
    std::unique_ptr<RenderGraph> m_renderGraph;

    FlatStringHashMap<std::unique_ptr<RenderNode>> m_renderNodes;
};
} // namespace crisp