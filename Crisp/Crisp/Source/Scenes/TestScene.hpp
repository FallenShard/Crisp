#pragma once

#include "Scene.hpp"

namespace crisp
{
    class Renderer;
    class Application;

    class TestScene : public Scene
    {
    public:
        TestScene(Renderer* renderer, Application* app);
        virtual ~TestScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt) override;
        virtual void render() override;
    };
}