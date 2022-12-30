#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
AbstractScene::AbstractScene(Application* application, Renderer* renderer)
    : m_app(application)
    , m_renderer(renderer)
    , m_resourceContext(std::make_unique<ResourceContext>(renderer))
    , m_renderGraph{std::make_unique<RenderGraph>(m_renderer)}
{
}

} // namespace crisp
