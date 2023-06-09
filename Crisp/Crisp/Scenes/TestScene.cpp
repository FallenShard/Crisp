#include <Crisp/Scenes/TestScene.hpp>

namespace crisp
{
TestScene::TestScene(Renderer* renderer, Window* window)
    : Scene(renderer, window)
{
}

TestScene::~TestScene() {}

void TestScene::resize(int /*width*/, int /*height*/) {}

void TestScene::update(float /*dt*/) {}

void TestScene::render() {}
} // namespace crisp