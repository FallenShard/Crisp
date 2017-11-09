#pragma once

#include <memory>

namespace crisp
{
    class FluidSimulation;

    class FluidSimulationScene
    {
    public:
        FluidSimulationScene();
        ~FluidSimulationScene();

        void resize(int width, int height);
        void update(float dt);

        void render() const;

    private:
        std::unique_ptr<FluidSimulation> m_fluidSimulation;
    };
}