#pragma once

namespace crisp
{
    class VulkanRenderer;
    class InputDispatcher;
    class Application;

    class IScene
    {
    public:
        virtual ~IScene() {}

        virtual void resize(int width, int height) = 0;
        virtual void update(float dt) = 0;
        virtual void render() = 0;
    };
}