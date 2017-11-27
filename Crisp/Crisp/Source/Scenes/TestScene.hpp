#pragma once

#include "Scene.hpp"

namespace crisp
{
    class VulkanRenderer;
    class Application;

    class TestScene : public Scene
    {
    public:
        TestScene(VulkanRenderer* renderer, Application* app);
        virtual ~TestScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt) override;
        virtual void render() override;
    };
}