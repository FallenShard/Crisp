#pragma once

#include <Crisp/Scenes/Scene.hpp>

namespace crisp
{
class TestScene : public AbstractScene
{
public:
    TestScene(Renderer* renderer, Application* app);
    ~TestScene() override;

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;
};
} // namespace crisp