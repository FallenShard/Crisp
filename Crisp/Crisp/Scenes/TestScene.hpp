#pragma once

#include <Crisp/Scenes/Scene.hpp>

namespace crisp {
class TestScene : public Scene {
public:
    TestScene(Renderer* renderer, Window* window);

    void resize(int width, int height) override;
    void update(const UpdateParams& updateParams) override;
    void render(const FrameContext& frameContext) override;
};
} // namespace crisp