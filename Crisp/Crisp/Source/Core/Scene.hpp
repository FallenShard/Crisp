#pragma once

namespace crisp
{
    class VulkanRenderer;

    class Scene
    {
    public:
        Scene(VulkanRenderer* renderer);
        ~Scene();

        void update(double dt);
        void render();

    private:
        VulkanRenderer* m_renderer;

    };
}
