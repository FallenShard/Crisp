#pragma once

#include "IScene.hpp"

namespace crisp
{
    class VulkanRenderer;
    class InputDispatcher;
    class Application;

    class TestScene : public IScene
    {
    public:
        TestScene(VulkanRenderer* renderer, InputDispatcher* inputDispatcher, Application* app);
        virtual ~TestScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt) override;
        virtual void render() override;
    };
}