#include "Scene.hpp"

#include "vulkan/VulkanRenderer.hpp"

namespace crisp
{
    Scene::Scene(VulkanRenderer* renderer)
        : m_renderer(renderer)
    {
        m_pipeline = std::make_unique<UniformColorPipeline>(m_renderer, &m_renderer->getDefaultRenderPass());
    }

    Scene::~Scene()
    {

    }

    void Scene::update(double dt)
    {

    }

    void Scene::render()
    {

    }
}