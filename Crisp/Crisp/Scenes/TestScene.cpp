#include <Crisp/Scenes/TestScene.hpp>

namespace crisp {
TestScene::TestScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {}

void TestScene::resize(int /*width*/, int /*height*/) {}

void TestScene::update(const UpdateParams&) {}

void TestScene::render() {}
} // namespace crisp