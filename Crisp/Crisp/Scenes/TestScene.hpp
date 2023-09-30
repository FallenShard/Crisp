#pragma once

#include <Crisp/Scenes/Scene.hpp>

namespace crisp {
class TestScene : public Scene {
public:
    TestScene(Renderer* renderer, Window* window);
    ~TestScene() override;

    void resize(int width, int height) override;
    void update(float dt) override;
    void render() override;
};
} // namespace crisp