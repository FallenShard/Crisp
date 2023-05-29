#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
AbstractScene::AbstractScene(Renderer* renderer, Window* window)
    : m_window(window)
    , m_renderer(renderer)
    , m_resourceContext(std::make_unique<ResourceContext>(m_renderer))
    , m_renderGraph{std::make_unique<RenderGraph>(m_renderer)}
{
}

} // namespace crisp
