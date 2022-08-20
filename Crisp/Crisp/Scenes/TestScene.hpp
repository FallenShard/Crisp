#pragma once

#include "Scene.hpp"

namespace crisp
{
    class Renderer;
    class Application;

    class TestScene : public AbstractScene
    {
    public:
        TestScene(Renderer* renderer, Application* app);
        ~TestScene() override;

        void resize(int width, int height) override;
        void update(float dt) override;
        void render() override;
    };
}