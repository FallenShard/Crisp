#pragma once

namespace crisp
{
    class Renderer;
    class InputDispatcher;
    class Application;

    class Scene
    {
    public:
        virtual ~Scene() {}

        virtual void resize(int width, int height) = 0;
        virtual void update(float dt) = 0;
        virtual void render() = 0;
    };
}