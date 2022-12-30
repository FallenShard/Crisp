#pragma once

#include <Crisp/Core/Application.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>

namespace crisp
{
class AbstractScene
{
public:
    AbstractScene(Application* application, Renderer* renderer);
    virtual ~AbstractScene() = default;

    virtual void resize(int width, int height) = 0;
    virtual void update(float dt) = 0;
    virtual void render() = 0;

protected:
    Application* m_app{nullptr};
    Renderer* m_renderer{nullptr};

    std::unique_ptr<ResourceContext> m_resourceContext;
    std::unique_ptr<RenderGraph> m_renderGraph;
};
} // namespace crisp