#pragma once

namespace crisp
{
    class AbstractScene
    {
    public:
        virtual ~AbstractScene() = default;

        virtual void resize(int width, int height) = 0;
        virtual void update(float dt) = 0;
        virtual void render() = 0;
    };
}